#include <catch2/catch_all.hpp>
#include "../src/network/Server.hpp"
#include "../src/network/RpcServer.hpp"
#include "../src/network/PeerServer.hpp"
#include "../src/network/MockSessionHandler.hpp"
#include "../src/network/MockAcceptor.hpp"
#include "../src/blockchain.hpp"
#include "../src/MockChunk.hpp"

namespace ssl = boost::asio::ssl;
using boost::asio::io_context;
using boost::asio::ip::tcp;

TEST_CASE("Server Construction", "[Server]")
{
    // Arrange
    boost::asio::io_context io_context;
    boost::asio::ssl::context ssl_context(ssl::context::sslv23);
    MockAcceptor acceptor(io_context);
    blockchain<MockChunk> bc(".");

    // Act
    Server<RpcServer, MockAcceptor> rpc(io_context, ssl_context, acceptor, bc);
    Server<PeerServer, MockAcceptor> node_server(io_context, ssl_context, acceptor, bc);

    // Assert
    REQUIRE(true);
}

TEST_CASE("Server uses SessionHandler correctly", "[Server]")
{
    std::string cert_file = "../ssl-cert-snakeoil.pem";
    std::string key_file = "../ssl-cert-snakeoil.key";
    // Arrange
    io_context io_context;
    ssl::context ssl_context(ssl::context::sslv23);
    ssl_context.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::single_dh_use);
    ssl_context.use_certificate_chain_file(cert_file);
    ssl_context.use_private_key_file(key_file, ssl::context::pem);
    MockAcceptor acceptor(io_context);
    blockchain<MockChunk> bc(".");
    Server<MockSessionHandler, MockAcceptor> RpcServer(io_context, ssl_context, acceptor, bc);

    SECTION("Server initializes SessionHandler on new connection")
    {
        // Act
        // Simulate a new connection; this might involve calling a method on your RpcServer
        // that simulates the acceptance of a new connection.
        RpcServer.start_accept(); // You'll need to replace this with actual code

        // Get the instance of the MockSessionHandler used by the Server
        auto last_session_handler = RpcServer.get_last_session_handler(); // This is hypothetical; implement accordingly

        // Assert
        // Verify that the MockSessionHandler's start method was called on the instance
        REQUIRE(last_session_handler->start_called_count == 1);
    }

    // Add more test sections as needed
}