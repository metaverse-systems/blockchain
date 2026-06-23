// MIT License
#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <memory>
#include <string>
#include <atomic>
#include "json.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class MetricsCollector;
class IBlockchain;
class PeerManager;

class MonitoringHttpServer {
public:
    MonitoringHttpServer(boost::asio::io_context &io_context,
                         ssl::context &ssl_context,
                         uint16_t port,
                         const std::string &bind_address,
                         IBlockchain &blockchain,
                         PeerManager *peer_manager,
                         MetricsCollector &metrics);

    void start();
    void stop();
    bool is_running() const { return running_.load(); }

private:
    boost::asio::io_context &io_context_;
    ssl::context &ssl_context_;
    tcp::acceptor acceptor_;
    IBlockchain &blockchain_;
    PeerManager *peer_manager_;
    MetricsCollector &metrics_;
    std::atomic<bool> running_{false};

    void do_accept();
    void handle_client(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream);
    void do_handshake(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream);
    void do_read_request(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream);
    void handle_request(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream, const std::string &request_line);
    void send_health_response(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream);
    void send_metrics_response(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream);
    void send_not_found(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream);
    void send_response(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream,
                       int status_code, const std::string &status_text,
                       const std::string &content_type, const std::string &body);
};
