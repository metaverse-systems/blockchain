#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <iostream>
#include <string>
#include <functional>
#include "../IBlockchain.hpp"
#include "../SyncState.hpp"
#include "PacketHeader.hpp"
#include "SyncMessages.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class PeerClient
{
  public:
    PeerClient(boost::asio::io_context &io_context,
               ssl::context &ssl_context,
               const std::string &host,
               unsigned short port,
               IBlockchain &bc,
               SyncStatus &sync_status)
        : resolver(io_context),
          socket(io_context, ssl_context),
          host(host),
          port(std::to_string(port)),
          bc(bc),
          sync_status(sync_status),
          chunk_timer(io_context) {}

    ssl::stream<tcp::socket> &get_socket_ref();
    void connect();
    void start_sync();
    bool is_connected() const { return connected; }

    template<typename T>
    void send(const T &obj, uint64_t packet_type);

  private:
    tcp::resolver resolver;
    ssl::stream<tcp::socket> socket;
    std::string host;
    std::string port;
    boost::asio::streambuf write_buffer;
    IBlockchain &bc;
    SyncStatus &sync_status;
    boost::asio::steady_timer chunk_timer;
    bool connected = false;
    std::vector<char> read_header_buf;
    std::vector<char> read_body_buf;

    void send_sync_query();
    void do_read_header();
    void do_read_body(const PacketHeader &header);
    void handle_sync_response(const SyncResponse &response);
    void abort_sync(const std::string &reason);
    void arm_chunk_timer();
    void cancel_chunk_timer();
};