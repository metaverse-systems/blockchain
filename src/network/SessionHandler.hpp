#pragma once

#include "../IBlockchain.hpp"
#include "../Chunk.hpp"
#include "../utils.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <chrono>

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class SessionHandler 
{
  protected:
    ssl::stream<tcp::socket> ssl_socket;
    IBlockchain &bc;
    boost::asio::steady_timer timeout_timer;
    std::chrono::seconds timeout_duration{30};

    SessionHandler(ssl::stream<tcp::socket> socket, IBlockchain &bc)
      : ssl_socket(std::move(socket)),
        bc(bc),
        timeout_timer(ssl_socket.get_executor()) {}

    virtual std::shared_ptr<SessionHandler> shared_self() = 0;
    virtual void on_handshake_complete() = 0;

    void arm_timer()
    {
        auto self = shared_self();
        timeout_timer.expires_after(timeout_duration);
        timeout_timer.async_wait([this, self](const boost::system::error_code &ec) {
            if (!ec) {
                logMessage("WARN", "Connection timed out after " +
                    std::to_string(timeout_duration.count()) + "s, closing");
                boost::system::error_code close_ec;
                ssl_socket.lowest_layer().close(close_ec);
            }
        });
    }

    void cancel_timer()
    {
        timeout_timer.cancel();
    }

  public:
    virtual ~SessionHandler() = default;

    void set_timeout(std::chrono::seconds timeout) { timeout_duration = timeout; }

    void start()
    {
        auto self = shared_self();
        arm_timer();
        ssl_socket.async_handshake(ssl::stream_base::server,
            [this, self](const boost::system::error_code &error) {
                cancel_timer();
                if (!error) {
                    this->on_handshake_complete();
                } else {
                    logMessage("ERROR", "SSL handshake failed: " + error.message());
                }
            }
        );
    }

    static std::shared_ptr<SessionHandler> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc);
    virtual ssl::stream<tcp::socket> &get_socket_ref() = 0;
};