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

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class PeerServer : public SessionHandler, public std::enable_shared_from_this<PeerServer>
{
  private:
    boost::asio::streambuf buffer;

  public:
    explicit PeerServer(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc);
    void start() override;
    static std::shared_ptr<PeerServer> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc);
    ssl::stream<tcp::socket> &get_socket_ref();

  private:
    void do_read_header();
    void do_read_body(const PacketHeader &header);
    void do_write();
};