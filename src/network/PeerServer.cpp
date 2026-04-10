#include "PeerServer.hpp"
#include "../Block.hpp"
#include "SyncMessages.hpp"
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
                        boost::archive::binary_iarchive ia(iss);
                        Block b;
                        ia >> b;
                        logMessage("INFO", "Received block #" + std::to_string(b.index));
                        b.dump();
                        break;
                    }
                    case PacketType::BLOCKCHAIN_QUERY:
                    {
                        boost::archive::binary_iarchive ia(iss);
                        SyncQuery query;
                        ia >> query;
                        logMessage("INFO", "Received BLOCKCHAIN_QUERY with local_chain_height=" + std::to_string(query.local_chain_height));
                        handle_blockchain_query(query);
                        return; // handler chains back to do_read_header
                    }
                    default:
                        logMessage("ERROR", "Received unknown packet type: " + std::to_string(header.type));
                        break;
                }

                do_read_header();
            } else {
                logMessage("ERROR", "Read body failed: " + ec.message());
            }
        });
}

void PeerServer::handle_blockchain_query(const SyncQuery &query)
{
    size_t total_height = bc.getChainBlockCount();

    if (query.local_chain_height >= total_height) {
        // Peer is up to date or ahead — send empty response
        SyncResponse response;
        response.total_chain_height = total_height;
        response.chunk_index = 0;
        // blocks left empty
        send_sync_response(response, 0, 0, total_height);
        return;
    }

    size_t start_chunk = query.local_chain_height / bc.chunkSize;
    size_t total_chunks = (total_height + bc.chunkSize - 1) / bc.chunkSize;

    // Send the first chunk
    SyncResponse response;
    response.total_chain_height = total_height;
    response.chunk_index = start_chunk;

    // Gather blocks for this chunk, starting from local_chain_height within the chunk
    size_t block_start = query.local_chain_height;
    size_t block_end = std::min((start_chunk + 1) * bc.chunkSize, total_height);

    for (size_t i = block_start; i < block_end; i++) {
        response.blocks.push_back(bc.getBlockByIndex(i));
    }

    size_t remaining = total_chunks - start_chunk - 1;
    send_sync_response(response, remaining, start_chunk + 1, total_height);
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
                logMessage("INFO", "Sent BLOCKCHAIN_RESPONSE chunk " + std::to_string(next_chunk > 0 ? next_chunk - 1 : 0));

                if (remaining_chunks > 0) {
                    // Build and send the next chunk
                    SyncResponse next_response;
                    next_response.total_chain_height = total_height;
                    next_response.chunk_index = next_chunk;

                    size_t block_start = next_chunk * bc.chunkSize;
                    size_t block_end = std::min((next_chunk + 1) * bc.chunkSize, static_cast<size_t>(total_height));

                    for (size_t i = block_start; i < block_end; i++) {
                        next_response.blocks.push_back(bc.getBlockByIndex(i));
                    }

                    send_sync_response(next_response, remaining_chunks - 1, next_chunk + 1, total_height);
                } else {
                    // All chunks sent — go back to reading
                    do_read_header();
                }
            } else {
                logMessage("ERROR", "Failed to send BLOCKCHAIN_RESPONSE: " + ec.message());
            }
        });
}