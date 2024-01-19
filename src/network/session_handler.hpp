#pragma once

#include "../blockchain.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class session_handler 
{
  protected:
    ssl::stream<tcp::socket> ssl_socket;
    blockchain &bc;
    session_handler(ssl::stream<tcp::socket> socket, blockchain &bc)
      : ssl_socket(std::move(socket)), bc(bc) {}
  public:
    virtual ~session_handler() = default;
    virtual void start() = 0;
    static std::shared_ptr<session_handler> create(boost::asio::io_context &io_context, ssl::context &ssl_context, blockchain &bc);
    virtual ssl::stream<tcp::socket> &get_socket_ref() = 0;
};