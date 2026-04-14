#include "PeerServer.hpp"
#include "../Block.hpp"
#include "../PeerManager.hpp"
#include "../BlockPropagation.hpp"
#include "SyncMessages.hpp"
#include "PeerMessages.hpp"
#include "PacketSerializer.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

PeerServer::PeerServer(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc)
        : SessionHandler(std::move(*socket_ptr), bc) {}

std::shared_ptr<PeerServer> PeerServer::create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc)
{
    std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream = std::make_shared<ssl::stream<tcp::socket>>(tcp::socket(io_context), ssl_context);
    return std::make_shared<PeerServer>(std::move(ssl_stream), bc);
}

ssl::stream<tcp::socket> &PeerServer::get_socket_ref()
{
    return ssl_socket;
};

void PeerServer::on_handshake_complete()
{
    // Check if inbound connection can be accepted
    if (peer_manager && !peer_manager->can_accept_inbound()) {
        LOG_WARN("Inbound connection limit reached, rejecting");
        boost::system::error_code ec;
        ssl_socket.lowest_layer().close(ec);
        return;
    }

    // Check if peer is banned
    if (peer_manager) {
        auto rhost = remote_host();
        auto rport = remote_port();
        if (peer_manager->is_banned(rhost, rport)) {
            LOG_WARN("Rejecting banned peer " + rhost + ":" + std::to_string(rport));
            boost::system::error_code ec;
            ssl_socket.lowest_layer().close(ec);
            return;
        }
        peer_manager->on_inbound_connected(rhost, rport, shared_from_this());
    }

    this->do_read_header();
}

void PeerServer::do_read_header()
{
    auto self(shared_from_this());
    boost::asio::async_read(this->ssl_socket, this->buffer,
        boost::asio::transfer_exactly(sizeof(PacketHeader)),
        [this, self](const boost::system::error_code& ec, std::size_t /*length*/) {
            if (!ec) {
                std::istream is(&this->buffer);
                PacketHeader header;
                is.read(reinterpret_cast<char*>(&header), sizeof(PacketHeader));

                this->do_read_body(header);
            } else {
                std::cerr << "Read header operation failed: " << ec.message() << std::endl;
            }
        });
}

void PeerServer::do_read_body(const PacketHeader &header) 
{
    auto self(shared_from_this());
    boost::asio::async_read(this->ssl_socket, this->buffer, boost::asio::transfer_exactly(header.length),
        [this, self, header](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                std::istream stream(&buffer);
                std::string serialized_str(header.length, 0);
                stream.read(&serialized_str[0], header.length);

                std::istringstream iss(serialized_str);
                switch(header.type)
                {
                    case PacketType::BLOCK:
                    {
                        try {
                            boost::archive::binary_iarchive ia(iss);
                            Block b;
                            ia >> b;
                            LOG_INFO("Received block #" + std::to_string(b.index));
                            if (block_propagation_) {
                                auto sender_key = remote_host() + ":" + std::to_string(remote_port());
                                block_propagation_->on_block_received(b, sender_key);
                            }
                        } catch (const std::exception &e) {
                            LOG_ERROR("Failed to deserialize BLOCK: " + std::string(e.what()));
                            if (peer_manager) {
                                peer_manager->increment_error(remote_host(), remote_port());
                            }
                        }
                        break;
                    }
                    case PacketType::BLOCKCHAIN_QUERY:
                    {
                        boost::archive::binary_iarchive ia(iss);
                        SyncQuery query;
                        ia >> query;
                        LOG_INFO("Received BLOCKCHAIN_QUERY with local_chain_height=" + std::to_string(query.local_chain_height));
                        handle_blockchain_query(query);
                        return; // handler chains back to do_read_header
                    }
                    case PacketType::PEER_EXCHANGE:
                    {
                        try {
                            boost::archive::binary_iarchive ia(iss);
                            PeerExchangeRequest request;
                            ia >> request;
                            LOG_INFO("Received PEER_EXCHANGE from uuid=" + request.sender_uuid
                                       + " peers=" + std::to_string(request.peers.size()));
                            handle_peer_exchange(request);
                        } catch (const std::exception &e) {
                            LOG_ERROR("Failed to deserialize PEER_EXCHANGE: " + std::string(e.what()));
                            if (peer_manager) {
                                peer_manager->increment_error(remote_host(), remote_port());
                            }
                        }
                        break;
                    }
                    case PacketType::PEER_EXCHANGE_RESPONSE:
                    {
                        try {
                            boost::archive::binary_iarchive ia(iss);
                            PeerExchangeResponse response;
                            ia >> response;
                            LOG_INFO("Received PEER_EXCHANGE_RESPONSE from uuid=" + response.sender_uuid);
                            if (peer_manager) {
                                peer_manager->on_peer_exchange_received(response.sender_uuid, response.sender_listen_port,
                                                                        remote_host(), response.peers);
                            }
                        } catch (const std::exception &e) {
                            LOG_ERROR("Failed to deserialize PEER_EXCHANGE_RESPONSE: " + std::string(e.what()));
                            if (peer_manager) {
                                peer_manager->increment_error(remote_host(), remote_port());
                            }
                        }
                        break;
                    }
                    default:
                        LOG_ERROR("Received unknown packet type: " + std::to_string(header.type));
                        break;
                }

                do_read_header();
            } else {
                LOG_ERROR("Read body failed: " + ec.message());
                if (peer_manager) {
                    peer_manager->increment_error(remote_host(), remote_port());
                    peer_manager->on_inbound_disconnected(remote_host(), remote_port());
                }
            }
        });
}

void PeerServer::handle_blockchain_query(const SyncQuery &query)
{
    size_t total_height = bc.getChainBlockCount();

    if (query.local_chain_height >= total_height) {
        SyncResponse response;
        response.total_chain_height = total_height;
        response.start_index = 0;
        send_sync_response(response, 0, 0, total_height);
        return;
    }

    size_t batch_size = 100;
    size_t block_start = query.local_chain_height;
    size_t block_end = std::min(block_start + batch_size, total_height);

    SyncResponse response;
    response.total_chain_height = total_height;
    response.start_index = block_start;

    for (size_t i = block_start; i < block_end; i++) {
        response.blocks.push_back(bc.getBlockByIndex(i));
    }

    size_t remaining_blocks = total_height - block_end;
    size_t remaining_batches = (remaining_blocks + batch_size - 1) / batch_size;
    send_sync_response(response, remaining_batches, block_end, total_height);
}

void PeerServer::send_sync_response(const SyncResponse &response, size_t remaining_chunks, size_t next_chunk, uint64_t total_height)
{
    auto self(shared_from_this());

    std::stringstream ss;
    boost::archive::binary_oarchive oa(ss);
    oa << response;
    std::string serialized = ss.str();

    PacketHeader header(serialized.size(), PacketType::BLOCKCHAIN_RESPONSE);
    auto header_buf = std::make_shared<std::vector<char>>(sizeof(header));
    std::memcpy(header_buf->data(), &header, sizeof(header));
    auto payload_buf = std::make_shared<std::string>(std::move(serialized));

    std::array<boost::asio::const_buffer, 2> buffers = {
        boost::asio::buffer(*header_buf),
        boost::asio::buffer(*payload_buf)
    };

    boost::asio::async_write(this->ssl_socket, buffers,
        [this, self, header_buf, payload_buf, remaining_chunks, next_chunk, total_height](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                LOG_INFO("Sent BLOCKCHAIN_RESPONSE batch starting at block " + std::to_string(next_chunk > 0 ? next_chunk - 100 : 0));

                if (remaining_chunks > 0) {
                    size_t batch_size = 100;
                    size_t block_start = next_chunk;
                    size_t block_end = std::min(block_start + batch_size, static_cast<size_t>(total_height));

                    SyncResponse next_response;
                    next_response.total_chain_height = total_height;
                    next_response.start_index = block_start;

                    for (size_t i = block_start; i < block_end; i++) {
                        next_response.blocks.push_back(bc.getBlockByIndex(i));
                    }

                    size_t remaining_blocks = total_height - block_end;
                    size_t remaining_batches = (remaining_blocks + batch_size - 1) / batch_size;
                    send_sync_response(next_response, remaining_batches, block_end, total_height);
                } else {
                    // All batches sent — go back to reading
                    do_read_header();
                }
            } else {
                LOG_ERROR("Failed to send BLOCKCHAIN_RESPONSE: " + ec.message());
            }
        });
}

void PeerServer::handle_peer_exchange(const PeerExchangeRequest &request)
{
    remote_uuid_ = request.sender_uuid;
    if (peer_manager) {
        peer_manager->on_peer_exchange_received(request.sender_uuid, request.sender_listen_port,
                                                 remote_host(), request.peers);
        peer_manager->check_duplicate_connection(request.sender_uuid, remote_host(), remote_port());
    }

    // Send response
    send_peer_exchange_response();
}

void PeerServer::send_peer_exchange_response()
{
    if (!peer_manager) {
        do_read_header();
        return;
    }

    PeerExchangeResponse response;
    response.sender_uuid = peer_manager->get_node_uuid();
    response.sender_listen_port = peer_manager->get_listen_port();
    response.peers = peer_manager->get_non_banned_peer_addresses();

    send_packet(response, PacketType::PEER_EXCHANGE_RESPONSE);
}

template<typename T>
void PeerServer::send_packet(const T &obj, uint64_t packet_type)
{
    auto self(shared_from_this());

    auto [header_data, serialized] = serialize_packet(obj, packet_type);

    auto header_buf = std::make_shared<std::vector<char>>(std::move(header_data));
    auto payload_buf = std::make_shared<std::string>(std::move(serialized));

    std::array<boost::asio::const_buffer, 2> buffers = {
        boost::asio::buffer(*header_buf),
        boost::asio::buffer(*payload_buf)
    };

    boost::asio::async_write(this->ssl_socket, buffers,
        [this, self, header_buf, payload_buf, packet_type](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                LOG_INFO("Sent packet type " + std::to_string(packet_type));
                do_read_header();
            } else {
                LOG_ERROR("Failed to send packet: " + ec.message());
            }
        });
}

std::string PeerServer::remote_host() const {
    try {
        return ssl_socket.lowest_layer().remote_endpoint().address().to_string();
    } catch (...) {
        return "unknown";
    }
}

uint16_t PeerServer::remote_port() const {
    try {
        return ssl_socket.lowest_layer().remote_endpoint().port();
    } catch (...) {
        return 0;
    }
}

// Explicit template instantiations
template void PeerServer::send_packet<Block>(const Block &obj, uint64_t packet_type);
template void PeerServer::send_packet<PeerExchangeResponse>(const PeerExchangeResponse &obj, uint64_t packet_type);