#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include "SessionHandler.hpp"
#include <iostream>
#include <string>
#include <vector>

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

class MockSessionHandler : public SessionHandler {
public:
    // Track calls to each method
    int start_called_count = 0;
    int get_socket_ref_called_count = 0;
    std::vector<std::string> received_data;

    // Constructor
    MockSessionHandler(std::shared_ptr<ssl::stream<tcp::socket>> socket_ptr, IBlockchain &bc)
    : SessionHandler(std::move(*socket_ptr), bc) {}

    // Override the start method to track calls and simulate behavior
    void start() override {
        start_called_count++;
        // Simulate behavior if needed, e.g., read/write to the socket
    }

    // Override the get_socket_ref method to track calls
    ssl::stream<tcp::socket> &get_socket_ref() override {
        get_socket_ref_called_count++;
        return ssl_socket; // Return the actual ssl_socket member from the base class
    }

    // Mock method to simulate receiving data (you can call this in your tests)
    void receiveData(const std::string& data) {
        received_data.push_back(data);
        // Optionally, trigger some behavior as if data was received over the network
    }

    static std::shared_ptr<MockSessionHandler> create(boost::asio::io_context &io_context, ssl::context &ssl_context, IBlockchain &bc)
    {
        std::shared_ptr<ssl::stream<tcp::socket>> ssl_stream = std::make_shared<ssl::stream<tcp::socket>>(tcp::socket(io_context), ssl_context);
        return std::make_shared<MockSessionHandler>(std::move(ssl_stream), bc);
    }
};
