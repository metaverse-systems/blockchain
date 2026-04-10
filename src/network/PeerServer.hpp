#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include "../IBlockchain.hpp"
#include "../Chunk.hpp"
#include "../json.hpp"
#include "SessionHandler.hpp"
#include "PacketHeader.hpp"
#include "SyncMessages.hpp"
#include "PeerMessages.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class PeerManager;
class BlockPropagation;

class PeerServer : public SessionHandler, public std::enable_shared_from_this<PeerServer>
{
  private:
    boost::asio::streambuf buffer;
    PeerManager *peer_manager = nullptr;
    BlockPropagation *block_propagation_ = nullptr;

  protected:
    std::shared_ptr<SessionHandler> shared_self() override { return shared_from_this(); }
    void on_handshake_complete() override;

  public:
    explicit PeerServer(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc);
    static std::shared_ptr<PeerServer> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc);
    ssl::stream<tcp::socket> &get_socket_ref();
    void set_peer_manager(PeerManager *pm) { peer_manager = pm; }
    void set_block_propagation(BlockPropagation *bp) { block_propagation_ = bp; }

    template<typename T>
    void send_packet_public(const T &obj, uint64_t packet_type) { send_packet(obj, packet_type); }

  private:
    void do_read_header();
    void do_read_body(const PacketHeader &header);
    void do_write();
    void handle_blockchain_query(const SyncQuery &query);
    void handle_peer_exchange(const PeerExchangeRequest &request);
    void send_peer_exchange_response();
    void send_sync_response(const SyncResponse &response, size_t remaining_chunks, size_t next_chunk, uint64_t total_height);

    template<typename T>
    void send_packet(const T &obj, uint64_t packet_type);

    std::string remote_host() const;
    uint16_t remote_port() const;
};