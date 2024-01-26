#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include <memory>
#include "../IBlockchain.hpp"
#include "../chunk.hpp"
#include "../json.hpp"
#include "session_handler.hpp"
#include "packet_header.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class p2p_server : public session_handler, public std::enable_shared_from_this<p2p_server>
{
  private:
    boost::asio::streambuf buffer;

  public:
    explicit p2p_server(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc);
    void start() override;
    static std::shared_ptr<p2p_server> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc);
    ssl::stream<tcp::socket> &get_socket_ref();

  private:
    void do_read_header();
    void do_read_body(const packet_header &header);
    void do_write();
};