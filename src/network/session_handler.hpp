#pragma once

#include "../IBlockchain.hpp"
#include "../chunk.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class session_handler 
{
  protected:
    ssl::stream<tcp::socket> ssl_socket;
    IBlockchain &bc;
    session_handler(ssl::stream<tcp::socket> socket, IBlockchain &bc)
      : ssl_socket(std::move(socket)), bc(bc) {}
  public:
    virtual ~session_handler() = default;
    virtual void start() = 0;
    static std::shared_ptr<session_handler> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc);
    virtual ssl::stream<tcp::socket> &get_socket_ref() = 0;
};