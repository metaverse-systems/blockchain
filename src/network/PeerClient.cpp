#include "PeerClient.hpp"
#include "PacketHeader.hpp"
#include "PacketSerializer.hpp"
#include "SyncMessages.hpp"
#include "PeerMessages.hpp"
#include "../PeerManager.hpp"
#include "../BlockPropagation.hpp"
#include "../utils.hpp"
#include "../ConsensusConfig.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <sstream>

ssl::stream<tcp::socket> &PeerClient::get_socket_ref()
{
    return socket;
}

void PeerClient::connect()
{
    auto endpoints = this->resolver.resolve(this->host, this->port);
    boost::asio::async_connect(this->socket.lowest_layer(), endpoints,
        [this](const boost::system::error_code &ec, tcp::endpoint)
        {
            if (!ec)
            {
                this->socket.async_handshake(ssl::stream_base::client,
                    [this](const boost::system::error_code &ec)
                    {
                        if (!ec)
                        {
                            LOG_INFO("Connected to peer " + host + ":" + port);
                            connected = true;
                            // Reset reconnect backoff on successful connection
                            if (peer_manager) {
                                peer_manager->reset_backoff(host, static_cast<uint16_t>(std::stoi(port)));
                            }
                            // Send peer exchange immediately after handshake
                            send_peer_exchange();
                            // Then start reading responses (peer exchange response + sync)
                            do_read_header();
                        }
                        else
                        {
                            LOG_ERROR("TLS handshake failed: " + ec.message());
                            handle_disconnect("TLS handshake failed");
                        }
                    });
            }
            else
            {
                LOG_ERROR("Connection to peer failed: " + ec.message());
                handle_disconnect("Connection failed");
            }
        });
}

void PeerClient::send_peer_exchange()
{
    if (!peer_manager) return;

    PeerExchangeRequest req;
    req.sender_uuid = peer_manager->get_node_uuid();
    req.sender_listen_port = peer_manager->get_listen_port();
    req.peers = peer_manager->get_non_banned_peer_addresses();

    LOG_INFO("Sending PEER_EXCHANGE to " + host + ":" + port + " with " + std::to_string(req.peers.size()) + " peers");
    send(req, PacketType::PEER_EXCHANGE);
}

void PeerClient::start_sync()
{
    if (sync_status.isSyncing.load()) {
        LOG_WARN("Sync already in progress, skipping");
        return;
    }

    sync_status.isSyncing.store(true);
    LOG_INFO("Starting chain sync with peer");
    send_sync_query();
}

void PeerClient::send_sync_query()
{
    SyncQuery query;
    query.local_chain_height = bc.getChainBlockCount();
    LOG_INFO("Sending BLOCKCHAIN_QUERY with local_chain_height=" + std::to_string(query.local_chain_height));

    send(query, PacketType::BLOCKCHAIN_QUERY);

    // Start reading response
    arm_chunk_timer();
    do_read_header();
}

void PeerClient::do_read_header()
{
    read_header_buf.resize(sizeof(PacketHeader));

    boost::asio::async_read(this->socket, boost::asio::buffer(read_header_buf),
        [this](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                cancel_chunk_timer();
                PacketHeader header;
                std::memcpy(&header, read_header_buf.data(), sizeof(PacketHeader));
                do_read_body(header);
            } else {
                if (sync_status.isSyncing.load()) {
                    abort_sync("Connection error while reading header: " + ec.message());
                }
                handle_disconnect("Read header error: " + ec.message());
            }
        });
}

void PeerClient::do_read_body(const PacketHeader &header)
{
    read_body_buf.resize(header.length);

    boost::asio::async_read(this->socket, boost::asio::buffer(read_body_buf),
        [this, header](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                std::string serialized_str(read_body_buf.begin(), read_body_buf.end());
                std::istringstream iss(serialized_str);

                switch (header.type)
                {
                    case PacketType::BLOCKCHAIN_RESPONSE:
                    {
                        boost::archive::binary_iarchive ia(iss);
                        SyncResponse response;
                        ia >> response;
                        handle_sync_response(response);
                        break;
                    }
                    case PacketType::BLOCK:
                    {
                        try {
                            boost::archive::binary_iarchive ia(iss);
                            Block b;
                            ia >> b;
                            LOG_INFO("Received block #" + std::to_string(b.index));
                            if (block_propagation_) {
                                auto sender_key = host + ":" + port;
                                block_propagation_->on_block_received(b, sender_key);
                            }
                        } catch (const std::exception &e) {
                            LOG_ERROR("Failed to deserialize BLOCK: " + std::string(e.what()));
                            if (peer_manager) {
                                peer_manager->increment_error(host, static_cast<uint16_t>(std::stoi(port)));
                            }
                        }
                        do_read_header();
                        return;
                    }
                    case PacketType::PEER_EXCHANGE_RESPONSE:
                    {
                        try {
                            boost::archive::binary_iarchive ia(iss);
                            PeerExchangeResponse response;
                            ia >> response;
                            handle_peer_exchange_response(response);
                        } catch (const std::exception &e) {
                            LOG_ERROR("Failed to deserialize PEER_EXCHANGE_RESPONSE: " + std::string(e.what()));
                            if (peer_manager) {
                                peer_manager->increment_error(host, static_cast<uint16_t>(std::stoi(port)));
                            }
                        }
                        do_read_header();
                        return;
                    }
                    case PacketType::PEER_EXCHANGE:
                    {
                        // As a client, we might receive a PEER_EXCHANGE from the server side
                        // during periodic exchanges; handle like a response
                        try {
                            boost::archive::binary_iarchive ia(iss);
                            PeerExchangeRequest req;
                            ia >> req;
                            if (peer_manager) {
                                peer_manager->on_peer_exchange_received(req.sender_uuid, req.sender_listen_port, host, req.peers);
                            }
                        } catch (const std::exception &e) {
                            LOG_ERROR("Failed to deserialize PEER_EXCHANGE: " + std::string(e.what()));
                            if (peer_manager) {
                                peer_manager->increment_error(host, static_cast<uint16_t>(std::stoi(port)));
                            }
                        }
                        do_read_header();
                        return;
                    }
                    default:
                        LOG_WARN("Received unknown packet type: " + std::to_string(header.type));
                        break;
                }
            } else {
                if (sync_status.isSyncing.load()) {
                    abort_sync("Connection error while reading body: " + ec.message());
                }
            }
        });
}

void PeerClient::handle_sync_response(const SyncResponse &response)
{
    LOG_INFO("Received BLOCKCHAIN_RESPONSE: chunk=" + std::to_string(response.chunk_index)
               + " blocks=" + std::to_string(response.blocks.size())
               + " total_chain_height=" + std::to_string(response.total_chain_height));

    size_t local_height = bc.getChainBlockCount();

    // Longest-chain guard: skip sync if peer is not strictly longer
    if (response.total_chain_height <= local_height) {
        LOG_INFO("Peer chain height (" + std::to_string(response.total_chain_height)
                   + ") is not longer than local (" + std::to_string(local_height) + "), sync not needed");
        sync_status.isSyncing.store(false);
        return;
    }

    // Empty response while expecting more blocks signals end-of-sync
    if (response.blocks.empty()) {
        if (response.total_chain_height > local_height) {
            LOG_WARN("Empty sync response while expecting more blocks (local="
                       + std::to_string(local_height) + " peer=" + std::to_string(response.total_chain_height) + ")");
        } else {
            LOG_INFO("Received empty sync response, chain is up to date");
        }
        sync_status.isSyncing.store(false);
        return;
    }

    // Validate each block in the chunk
    const auto &config = bc.getConfig();
    for (size_t i = 0; i < response.blocks.size(); i++) {
        const Block &block = response.blocks[i];

        if (block.index == 0) {
            // Genesis block — skip PoW validation
            continue;
        }

        // Get the previous block for validation
        Block prev_block;
        if (i == 0) {
            // First block in chunk — get previous from local chain
            if (block.index > 0 && block.index - 1 < local_height) {
                prev_block = bc.getBlockByIndex(block.index - 1);
            } else {
                LOG_ERROR("Cannot validate block " + std::to_string(block.index)
                           + ": no previous block available");
                abort_sync("Missing previous block for validation at index " + std::to_string(block.index));
                return;
            }
        } else {
            prev_block = response.blocks[i - 1];
        }

        if (!IBlockchain::isValidNewBlock(block, prev_block, config)) {
            LOG_ERROR("Block " + std::to_string(block.index)
                       + " failed validation in chunk " + std::to_string(response.chunk_index)
                       + " from peer " + host + ":" + port);
            abort_sync("Invalid block at index " + std::to_string(block.index));
            return;
        }
    }

    LOG_INFO("Chunk " + std::to_string(response.chunk_index) + " validated successfully ("
               + std::to_string(response.blocks.size()) + " blocks)");

    // Persist the valid chunk: append blocks to the chain
    for (const auto &block : response.blocks) {
        if (block.index < local_height) {
            // Overlap region: verify hash matches local chain
            Block local_block = bc.getBlockByIndex(block.index);
            if (local_block.hash != block.hash) {
                abort_sync("Overlap hash mismatch at block " + std::to_string(block.index));
                return;
            }
            continue; // Already have this block
        }
        bc.appendBlock(block);
    }

    // Save the chunk and keys
    size_t chunk_idx = response.chunk_index;
    try {
        bc.saveChunk(chunk_idx);
        bc.saveKeys();
    } catch (...) {
        // Chunk may not exist yet in the chain structure — that's ok for now
    }

    size_t new_local_height = bc.getChainBlockCount();
    LOG_INFO("Synced " + std::to_string(response.blocks.size())
               + " blocks, local height now " + std::to_string(new_local_height));

    // Check if we need more chunks
    if (new_local_height < response.total_chain_height) {
        // Request next chunk
        arm_chunk_timer();
        do_read_header();
    } else {
        LOG_INFO("Chain sync complete, local height=" + std::to_string(new_local_height));
        sync_status.isSyncing.store(false);
        if (block_propagation_) {
            block_propagation_->process_sync_queue();
        }
    }
}

void PeerClient::abort_sync(const std::string &reason)
{
    LOG_ERROR("Sync aborted: " + reason);
    cancel_chunk_timer();
    sync_status.isSyncing.store(false);
}

void PeerClient::handle_peer_exchange_response(const PeerExchangeResponse &response)
{
    remote_uuid_ = response.sender_uuid;
    LOG_INFO("Received PEER_EXCHANGE_RESPONSE from " + host + ":" + port
               + " uuid=" + response.sender_uuid + " peers=" + std::to_string(response.peers.size()));

    if (peer_manager) {
        peer_manager->on_peer_exchange_received(response.sender_uuid, response.sender_listen_port, host, response.peers);
        peer_manager->check_duplicate_connection(response.sender_uuid, host, static_cast<uint16_t>(std::stoi(port)));
    }
}

void PeerClient::handle_disconnect(const std::string &reason)
{
    if (!connected && !socket.lowest_layer().is_open()) return;
    connected = false;
    LOG_INFO("Peer " + host + ":" + port + " disconnected: " + reason);

    boost::system::error_code ec;
    socket.lowest_layer().close(ec);

    if (peer_manager) {
        peer_manager->on_peer_disconnected(host, static_cast<uint16_t>(std::stoi(port)));
    }
}

void PeerClient::arm_chunk_timer()
{
    chunk_timer.expires_after(std::chrono::seconds(60));
    chunk_timer.async_wait([this](const boost::system::error_code &ec) {
        if (!ec) {
            // Timer fired — timeout
            abort_sync("Chunk response timeout (60s)");
            boost::system::error_code close_ec;
            socket.lowest_layer().close(close_ec);
        }
    });
}

void PeerClient::cancel_chunk_timer()
{
    chunk_timer.cancel();
}

template<typename T>
void PeerClient::send(const T &obj, uint64_t packet_type)
{
    auto [header_data, serialized_str] = serialize_packet(obj, packet_type);

    std::ostream stream(&this->write_buffer);
    stream.write(header_data.data(), header_data.size());
    stream << serialized_str;

    boost::asio::async_write(this->socket, this->write_buffer,
        [this](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                LOG_INFO("Packet sent successfully");
            } else {
                LOG_ERROR("Error sending packet: " + ec.message());
            }
        });
}

// Explicit template instantiations
template void PeerClient::send<Block>(const Block &obj, uint64_t packet_type);
template void PeerClient::send<SyncQuery>(const SyncQuery &obj, uint64_t packet_type);
template void PeerClient::send<PeerExchangeRequest>(const PeerExchangeRequest &obj, uint64_t packet_type);
template void PeerClient::send<PeerExchangeResponse>(const PeerExchangeResponse &obj, uint64_t packet_type);