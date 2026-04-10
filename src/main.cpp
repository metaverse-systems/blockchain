#include <iostream>
#include <boost/asio.hpp>
#include <chrono>
#include <cstdlib>
#include "Block.hpp"
#include "Blockchain.hpp"
#include "utils.hpp"
#include "network/Server.hpp"
#include "network/RpcServer.hpp"
#include "network/PeerServer.hpp"
#include "network/MockSessionHandler.hpp"

using boost::asio::ip::tcp;

int main(int argc, char *argv[])
{
    if(argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <path to blockchain directory>" << std::endl;
        return 1;
    }

    std::filesystem::path blockchainDir(argv[1]);

    // Load .env file from blockchain data directory
    loadDotEnv(blockchainDir / ".env");

    // Validate required TLS environment variables
    const char *cert_file_env = std::getenv("BLOCKCHAIN_CERT_FILE");
    const char *key_file_env = std::getenv("BLOCKCHAIN_KEY_FILE");
    const char *ca_file_env = std::getenv("BLOCKCHAIN_CA_FILE");
    const char *timeout_env = std::getenv("BLOCKCHAIN_TIMEOUT");

    if (!cert_file_env || std::string(cert_file_env).empty()) {
        logMessage("ERROR", "BLOCKCHAIN_CERT_FILE environment variable is not set");
        return 1;
    }
    if (!key_file_env || std::string(key_file_env).empty()) {
        logMessage("ERROR", "BLOCKCHAIN_KEY_FILE environment variable is not set");
        return 1;
    }

    std::string cert_file(cert_file_env);
    std::string key_file(key_file_env);
    std::string ca_file = ca_file_env ? std::string(ca_file_env) : "";

    int timeout_seconds = 30;
    if (timeout_env) {
        try {
            timeout_seconds = std::stoi(timeout_env);
        } catch (...) {
            logMessage("WARN", "Invalid BLOCKCHAIN_TIMEOUT value, using default 30s");
        }
    }

    Blockchain<Chunk> bc(blockchainDir);
    bc.loadChunk(0);
    bc.loadKeys();
    bc.dumpBlocks();

    unsigned short port = 12345;

    boost::asio::io_context io_context;

    // Mutual TLS context for P2P
    boost::asio::ssl::context p2p_ssl_context(ssl::context::tlsv12);
    p2p_ssl_context.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::single_dh_use);
    p2p_ssl_context.use_certificate_chain_file(cert_file);
    p2p_ssl_context.use_private_key_file(key_file, ssl::context::pem);
    p2p_ssl_context.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
    if (!ca_file.empty()) {
        p2p_ssl_context.load_verify_file(ca_file);
    }

    // Server-only TLS context for RPC
    boost::asio::ssl::context rpc_ssl_context(ssl::context::tlsv12);
    rpc_ssl_context.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::single_dh_use);
    rpc_ssl_context.use_certificate_chain_file(cert_file);
    rpc_ssl_context.use_private_key_file(key_file, ssl::context::pem);

    tcp::acceptor rpc_acceptor(io_context);
    tcp::endpoint endpoint(tcp::v6(), port);
    rpc_acceptor.open(endpoint.protocol());
    rpc_acceptor.set_option(tcp::acceptor::reuse_address(true));
    rpc_acceptor.set_option(boost::asio::ip::v6_only(false));
    rpc_acceptor.bind(endpoint);
    rpc_acceptor.listen();

    Server<RpcServer, tcp::acceptor> rpc(io_context, rpc_ssl_context, rpc_acceptor, bc);
    rpc.set_timeout(std::chrono::seconds(timeout_seconds));
    rpc.start_accept();

    tcp::acceptor p2p_acceptor(io_context);
    tcp::endpoint p2p_endpoint(tcp::v6(), static_cast<unsigned short>(port + 1));
    p2p_acceptor.open(p2p_endpoint.protocol());
    p2p_acceptor.set_option(tcp::acceptor::reuse_address(true));
    p2p_acceptor.set_option(boost::asio::ip::v6_only(false));
    p2p_acceptor.bind(p2p_endpoint);
    p2p_acceptor.listen();

    Server<PeerServer, tcp::acceptor> node_server(io_context, p2p_ssl_context, p2p_acceptor, bc);
    node_server.set_timeout(std::chrono::seconds(timeout_seconds));
    node_server.start_accept();

    io_context.run();
  
    return 0;
}
