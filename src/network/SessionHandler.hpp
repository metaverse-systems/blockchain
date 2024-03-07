#pragma once

#include "../IBlockchain.hpp"
#include "../Chunk.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class SessionHandler 
{
  protected:
    ssl::stream<tcp::socket> ssl_socket;
    IBlockchain &bc;
    SessionHandler(ssl::stream<tcp::socket> socket, IBlockchain &bc)
      : ssl_socket(std::move(socket)), bc(bc) {}
  public:
    virtual ~SessionHandler() = default;
    virtual void start() = 0;
    static std::shared_ptr<SessionHandler> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc);
    virtual ssl::stream<tcp::socket> &get_socket_ref() = 0;
};