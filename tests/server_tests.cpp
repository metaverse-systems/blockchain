#include <catch2/catch_all.hpp>
#include "../src/network/server.hpp"
#include "../src/network/rpc_server.hpp"
#include "../src/network/p2p_server.hpp"
#include "../src/network/MockSessionHandler.hpp"
#include "../src/network/MockAcceptor.hpp"
#include "../src/blockchain.hpp"
#include "../src/MockChunk.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::io_context;
using boost::asio::ip::tcp;

TEST_CASE("Server Construction", "[server]")
{
    // Arrange
    boost::asio::io_context io_context;
    boost::asio::ssl::context ssl_context(ssl::context::sslv23);
    MockAcceptor acceptor(io_context);
    blockchain<MockChunk> bc(".");

    // Act
    server<rpc_server, MockAcceptor> rpc(io_context, ssl_context, acceptor, bc);
    server<p2p_server, MockAcceptor> node_server(io_context, ssl_context, acceptor, bc);

    // Assert
    REQUIRE(true);
}

TEST_CASE("Server uses SessionHandler correctly", "[server]")
{
    // Arrange
    io_context io_context;
    ssl::context ssl_context(ssl::context::sslv23);
    MockAcceptor acceptor(io_context);
    blockchain<MockChunk> bc(".");
    server<MockSessionHandler, MockAcceptor> test_server(io_context, ssl_context, acceptor, bc);

    SECTION("Server initializes SessionHandler on new connection")
    {
        // Act
        // Simulate a new connection; this might involve calling a method on your test_server
        // that simulates the acceptance of a new connection.
        test_server.start_accept(); // You'll need to replace this with actual code

        // Get the instance of the MockSessionHandler used by the server
        auto last_session_handler = test_server.get_last_session_handler(); // This is hypothetical; implement accordingly

        // Assert
        // Verify that the MockSessionHandler's start method was called on the instance
        REQUIRE(last_session_handler->start_called_count == 1);
    }

    // Add more test sections as needed
}