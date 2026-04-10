#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "SessionHandler.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class MockSessionHandler : public SessionHandler, public std::enable_shared_from_this<MockSessionHandler> {
public:
    // Track calls to each method
    int start_called_count = 0;
    int handshake_complete_called_count = 0;
    int get_socket_ref_called_count = 0;
    std::vector<std::string> received_data;

    // Constructor
    MockSessionHandler(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc)
    : SessionHandler(std::move(*socket_ptr), bc) {}

protected:
    std::shared_ptr<SessionHandler> shared_self() override { return shared_from_this(); }

    void on_handshake_complete() override {
        handshake_complete_called_count++;
    }

public:
    // Override start to track calls and skip real SSL handshake in mocks
    void start() {
        start_called_count++;
        on_handshake_complete();
    }

    // Override the get_socket_ref method to track calls
    ssl::stream<tcp::socket> &get_socket_ref() override {
        get_socket_ref_called_count++;
        return ssl_socket;
    }

    // Mock method to simulate receiving data
    void receiveData(const std::string& data) {
        received_data.push_back(data);
    }

    static std::shared_ptr<MockSessionHandler> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc)
    {
        std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream = std::make_shared<ssl::stream<tcp::socket>>(tcp::socket(io_context), ssl_context);
        return std::make_shared<MockSessionHandler>(std::move(ssl_stream), bc);
    }
};
