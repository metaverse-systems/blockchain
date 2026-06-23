// MIT License
#include "MonitoringHttpServer.hpp"
#include "MetricsCollector.hpp"
#include "IBlockchain.hpp"
#include "PeerManager.hpp"
#include "utils.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <chrono>
#include <algorithm>

namespace asio = boost::asio;
namespace ssl = boost::asio::ssl;
using asio::ip::tcp;

MonitoringHttpServer::MonitoringHttpServer(asio::io_context &io_context,
                                           ssl::context &ssl_context,
                                           uint16_t port,
                                           const std::string &bind_address,
                                           IBlockchain &blockchain,
                                           PeerManager *peer_manager,
                                           MetricsCollector &metrics)
    : io_context_(io_context),
      ssl_context_(ssl_context),
      acceptor_(io_context),
      blockchain_(blockchain),
      peer_manager_(peer_manager),
      metrics_(metrics)
{
    // Determine address family from bind address
    tcp::resolver resolver(io_context);
    auto endpoints = resolver.resolve(bind_address, std::to_string(port));
    tcp::endpoint ep = *endpoints.begin();

    acceptor_.open(ep.protocol());
    acceptor_.set_option(tcp::acceptor::reuse_address(true));
    acceptor_.bind(ep);
    acceptor_.listen();

    metrics_.blockchain_ = &blockchain;
    metrics_.peer_manager_ = peer_manager;

    LOG_INFO("Monitoring server configured to listen on " + bind_address + ":" + std::to_string(port));
}

void MonitoringHttpServer::start() {
    running_.store(true);
    LOG_INFO("Monitoring HTTPS server starting");
    do_accept();
}

void MonitoringHttpServer::stop() {
    running_.store(false);
    boost::system::error_code ec;
    acceptor_.close(ec);
    LOG_INFO("Monitoring HTTPS server stopped");
}

void MonitoringHttpServer::do_accept() {
    if (!running_.load()) return;

    auto ssl_stream = std::make_shared<ssl::stream<tcp::socket>>(tcp::socket(io_context_), ssl_context_);
    acceptor_.async_accept(
        ssl_stream->lowest_layer(),
        [this, ssl_stream](const boost::system::error_code &ec) {
            if (!ec) {
                handle_client(ssl_stream);
            } else {
                if (running_.load()) {
                    LOG_WARN("Monitoring accept failed: " + ec.message());
                }
            }
            do_accept();
        });
}

void MonitoringHttpServer::handle_client(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream) {
    do_handshake(ssl_stream);
}

void MonitoringHttpServer::do_handshake(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream) {
    auto self = ssl_stream;
    ssl_stream->async_handshake(ssl::stream_base::server,
        [this, self](const boost::system::error_code &ec) {
            if (!ec) {
                do_read_request(self);
            } else {
                LOG_WARN("Monitoring TLS handshake failed: " + ec.message());
            }
        });
}

void MonitoringHttpServer::do_read_request(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream) {
    auto buf = std::make_shared<asio::streambuf>();
    asio::async_read_until(*ssl_stream, *buf, "\r\n\r\n",
        [this, ssl_stream, buf](const boost::system::error_code &ec, std::size_t) {
            if (ec) {
                LOG_WARN("Monitoring read failed: " + ec.message());
                return;
            }
            std::istream is(buf.get());
            std::string request_line;
            std::getline(is, request_line);
            // Consume remaining headers
            std::string header;
            while (std::getline(is, header) && !header.empty()) {}
            handle_request(ssl_stream, request_line);
        });
}

void MonitoringHttpServer::handle_request(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream, const std::string &request_line) {
    // Parse: GET /path HTTP/1.1
    if (request_line.substr(0, 4) != "GET ") {
        send_not_found(ssl_stream);
        return;
    }

    std::string path = request_line.substr(4);
    auto space_pos = path.find(' ');
    if (space_pos != std::string::npos) {
        path = path.substr(0, space_pos);
    }

    if (path == "/health") {
        send_health_response(ssl_stream);
    } else if (path == "/metrics") {
        send_metrics_response(ssl_stream);
    } else {
        send_not_found(ssl_stream);
    }
}

void MonitoringHttpServer::send_health_response(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream) {
    nlohmann::json response;
    response["status"] = blockchain_.isShuttingDown() ? "shutting_down" : "healthy";
    response["chain_height"] = static_cast<int64_t>(blockchain_.getChainLength());

    int64_t peer_count = 0;
    if (peer_manager_) {
        peer_count = static_cast<int64_t>(peer_manager_->get_peers().size());
    }
    response["peer_count"] = peer_count;
    response["chunk_count"] = static_cast<int64_t>(blockchain_.getChunkCount());
    response["uptime_seconds"] = metrics_.uptime_seconds();

    // last_block_index: chain_length - 1, or -1 if empty
    int64_t chain_length = static_cast<int64_t>(blockchain_.getChainLength());
    response["last_block_index"] = (chain_length > 0) ? chain_length - 1 : -1;

    send_response(ssl_stream, 200, "OK", "application/json", response.dump(2));
}

void MonitoringHttpServer::send_metrics_response(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream) {
    std::string body = metrics_.generatePrometheusText();
    send_response(ssl_stream, 200, "OK", "text/plain; version=0.0.4", body);
}

void MonitoringHttpServer::send_not_found(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream) {
    send_response(ssl_stream, 404, "Not Found", "text/plain", "Not Found\n");
}

void MonitoringHttpServer::send_response(std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream,
                                          int status_code, const std::string &status_text,
                                          const std::string &content_type, const std::string &body) {
    std::ostringstream oss;
    oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n"
        << "Content-Type: " << content_type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;

    asio::async_write(*ssl_stream, asio::buffer(oss.str()),
        [ssl_stream](const boost::system::error_code &ec, std::size_t) {
            // Close connection after response
            boost::system::error_code close_ec;
            ssl_stream->lowest_layer().close(close_ec);
        });
}
