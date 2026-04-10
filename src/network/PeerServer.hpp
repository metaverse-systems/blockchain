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

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class PeerServer : public SessionHandler, public std::enable_shared_from_this<PeerServer>
{
  private:
    boost::asio::streambuf buffer;

  protected:
    std::shared_ptr<SessionHandler> shared_self() override { return shared_from_this(); }
    void on_handshake_complete() override;

  public:
    explicit PeerServer(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc);
    static std::shared_ptr<PeerServer> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc);
    ssl::stream<tcp::socket> &get_socket_ref();

  private:
    void do_read_header();
    void do_read_body(const PacketHeader &header);
    void do_write();
    void handle_blockchain_query(const SyncQuery &query);
    void send_sync_response(const SyncResponse &response, size_t remaining_chunks, size_t next_chunk, uint64_t total_height);
};