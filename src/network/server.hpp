#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include "../IBlockchain.hpp"
#include "../Chunk.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

template<typename SessionHandler, typename Acceptor>
class server
{
  private:
    boost::asio::io_context &io_context;
    ssl::context &ssl_context;
    Acceptor &acceptor;
    IBlockchain &bc;
    std::shared_ptr<SessionHandler> last_session_handler;

  public:
    server(boost::asio::io_context &io_context, ssl::context &ssl_context, Acceptor &acceptor, IBlockchain &bc)
        : io_context(io_context),
          ssl_context(ssl_context),
          acceptor(acceptor),
          bc(bc)
    {
    }

    void start_accept()
    {
        auto new_session = SessionHandler::create(io_context, ssl_context, bc);
        this->last_session_handler = new_session;

        acceptor.async_accept(new_session->get_socket_ref().lowest_layer(),
        [this, new_session](const boost::system::error_code& error)
        {
            if (!error) {
                new_session->start();
            } else {
                std::cerr << "Accept failed: " << error.message() << std::endl;
            }
            this->start_accept();
        });
    }

    std::shared_ptr<SessionHandler> get_last_session_handler() const
    {
        return this->last_session_handler;
    }
};