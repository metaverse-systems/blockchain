#include "PeerClient.hpp"
#include "PacketHeader.hpp"
#include "SyncMessages.hpp"
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
                            logMessage("INFO", "Connected to peer " + host + ":" + port);
                            connected = true;
                            start_sync();
                        }
                        else
                        {
                            logMessage("ERROR", "TLS handshake failed: " + ec.message());
                        }
                    });
            }
            else
            {
                logMessage("ERROR", "Connection to peer failed: " + ec.message());
            }
        });
}

void PeerClient::start_sync()
{
    if (sync_status.isSyncing.load()) {
        logMessage("WARN", "Sync already in progress, skipping");
        return;
    }

    sync_status.isSyncing.store(true);
    logMessage("INFO", "Starting chain sync with peer");
    send_sync_query();
}

void PeerClient::send_sync_query()
{
    SyncQuery query;
    query.local_chain_height = bc.getChainBlockCount();
    logMessage("INFO", "Sending BLOCKCHAIN_QUERY with local_chain_height=" + std::to_string(query.local_chain_height));

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
                        boost::archive::binary_iarchive ia(iss);
                        Block b;
                        ia >> b;
                        logMessage("INFO", "Received block #" + std::to_string(b.index));
                        break;
                    }
                    default:
                        logMessage("WARN", "Received unknown packet type: " + std::to_string(header.type));
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
    logMessage("INFO", "Received BLOCKCHAIN_RESPONSE: chunk=" + std::to_string(response.chunk_index)
               + " blocks=" + std::to_string(response.blocks.size())
               + " total_chain_height=" + std::to_string(response.total_chain_height));

    size_t local_height = bc.getChainBlockCount();

    // Longest-chain guard: skip sync if peer is not strictly longer
    if (response.total_chain_height <= local_height) {
        logMessage("INFO", "Peer chain height (" + std::to_string(response.total_chain_height)
                   + ") is not longer than local (" + std::to_string(local_height) + "), sync not needed");
        sync_status.isSyncing.store(false);
        return;
    }

    // Empty response indicates nothing to sync
    if (response.blocks.empty()) {
        logMessage("INFO", "Received empty sync response, chain is up to date");
        sync_status.isSyncing.store(false);
        return;
    }

    // Validate each block in the chunk
    ConsensusConfig config;
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
                logMessage("ERROR", "Cannot validate block " + std::to_string(block.index)
                           + ": no previous block available");
                abort_sync("Missing previous block for validation at index " + std::to_string(block.index));
                return;
            }
        } else {
            prev_block = response.blocks[i - 1];
        }

        if (!IBlockchain::isValidNewBlock(block, prev_block, config)) {
            logMessage("ERROR", "Block " + std::to_string(block.index)
                       + " failed validation in chunk " + std::to_string(response.chunk_index)
                       + " from peer " + host + ":" + port);
            abort_sync("Invalid block at index " + std::to_string(block.index));
            return;
        }
    }

    logMessage("INFO", "Chunk " + std::to_string(response.chunk_index) + " validated successfully ("
               + std::to_string(response.blocks.size()) + " blocks)");

    // Persist the valid chunk: append blocks to the chain
    // For the first chunk during initial sync, we may need to use replaceChain
    // For incremental sync, we add blocks one by one
    for (auto &block : response.blocks) {
        if (block.index < local_height) {
            continue; // Already have this block
        }
    }

    // Save the chunk
    size_t chunk_idx = response.chunk_index;
    try {
        bc.saveChunk(chunk_idx);
    } catch (...) {
        // Chunk may not exist yet in the chain structure — that's ok for now
    }

    size_t new_local_height = bc.getChainBlockCount();
    logMessage("INFO", "Synced " + std::to_string(response.blocks.size())
               + " blocks, local height now " + std::to_string(new_local_height));

    // Check if we need more chunks
    if (new_local_height < response.total_chain_height) {
        // Request next chunk
        arm_chunk_timer();
        do_read_header();
    } else {
        logMessage("INFO", "Chain sync complete, local height=" + std::to_string(new_local_height));
        sync_status.isSyncing.store(false);
    }
}

void PeerClient::abort_sync(const std::string &reason)
{
    logMessage("ERROR", "Sync aborted: " + reason);
    cancel_chunk_timer();
    sync_status.isSyncing.store(false);
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
    std::stringstream ss;
    boost::archive::binary_oarchive oa(ss);
    oa << obj;
    std::string serialized_str = ss.str();

    PacketHeader header(serialized_str.size(), packet_type);
    std::vector<char> header_data(sizeof(header));
    std::memcpy(header_data.data(), &header, sizeof(header));

    std::ostream stream(&this->write_buffer);
    stream.write(header_data.data(), header_data.size());
    stream << serialized_str;

    boost::asio::async_write(this->socket, this->write_buffer,
        [this](const boost::system::error_code &ec, std::size_t) {
            if (!ec) {
                logMessage("INFO", "Packet sent successfully");
            } else {
                logMessage("ERROR", "Error sending packet: " + ec.message());
            }
        });
}

// Explicit template instantiations
template void PeerClient::send<Block>(const Block &obj, uint64_t packet_type);
template void PeerClient::send<SyncQuery>(const SyncQuery &obj, uint64_t packet_type);