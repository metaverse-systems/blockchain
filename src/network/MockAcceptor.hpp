#pragma once

#include <boost/asio.hpp>

namespace ssl = boost::asio::ssl;

class MockAcceptor {
    bool accepted = false;
public:
    MockAcceptor(boost::asio::io_context &io_context) {}

    void async_accept(ssl::stream<tcp::socket>::lowest_layer_type &socket, std::function<void(const boost::system::error_code &)> handler) {
        if (!accepted) {
            accepted = true;
            handler(boost::system::error_code{});
        }
    }
};