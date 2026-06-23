#pragma once
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <iostream>
#include "../IBlockchain.hpp"
#include "../Chunk.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class PeerManager;
class BlockPropagation;
class MetricsCollector;

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
    BlockPropagation *block_propagation = nullptr;
    MetricsCollector *metrics_collector = nullptr;
    std::vector<std::string> allowed_streams;

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
    void set_block_propagation(BlockPropagation *bp) { block_propagation = bp; }
    void set_metrics_collector(MetricsCollector *mc) { metrics_collector = mc; }
    void set_allowed_streams(const std::vector<std::string> &streams) { allowed_streams = streams; }

    void start_accept()
    {
        auto new_session = SessionHandler::create(io_context, ssl_context, bc);
        new_session->set_timeout(timeout_duration);
        if (peer_manager) {
            new_session->set_peer_manager(peer_manager);
        }
        if constexpr (requires(SessionHandler &s, BlockPropagation *bp) { s.set_block_propagation(bp); }) {
            if (block_propagation) {
                new_session->set_block_propagation(block_propagation);
            }
        }
        if constexpr (requires(SessionHandler &s, const std::vector<std::string> &v) { s.set_allowed_streams(v); }) {
            if (!allowed_streams.empty()) {
                new_session->set_allowed_streams(allowed_streams);
            }
        }        if constexpr (requires(SessionHandler &s, MetricsCollector *mc) { s.set_metrics_collector(mc); }) {
            if (metrics_collector) {
                new_session->set_metrics_collector(metrics_collector);
            }
        }
        acceptor.async_accept(new_session->get_socket_ref().lowest_layer(),
        [this, new_session](const boost::system::error_code& error)
        {
            if (!error) {
                new_session->start();
                this->last_session_handler = new_session;
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