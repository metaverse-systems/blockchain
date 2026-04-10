#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include "../IBlockchain.hpp"
#include "../Chunk.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class PeerManager;

template<typename SessionHandler, typename Acceptor>
class Server
{
  private:
    boost::asio::io_context &io_context;
    ssl::context &ssl_context;
    Acceptor &acceptor;
    IBlockchain &bc;
    std::shared_ptr<SessionHandler> last_session_handler;
    std::chrono::seconds timeout_duration{30};
    PeerManager *peer_manager = nullptr;

  public:
    Server(boost::asio::io_context &io_context, ssl::context &ssl_context, Acceptor &acceptor, IBlockchain &bc)
        : io_context(io_context),
          ssl_context(ssl_context),
          acceptor(acceptor),
          bc(bc)
    {
    }

    void set_timeout(std::chrono::seconds timeout) { timeout_duration = timeout; }
    void set_peer_manager(PeerManager *pm) { peer_manager = pm; }

    void start_accept()
    {
        auto new_session = SessionHandler::create(io_context, ssl_context, bc);
        new_session->set_timeout(timeout_duration);
        if (peer_manager) {
            new_session->set_peer_manager(peer_manager);
        }
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