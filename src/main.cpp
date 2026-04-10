#include <iostream>
#include <boost/asio.hpp>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include "Block.hpp"
#include "Blockchain.hpp"
#include "ConsensusConfig.hpp"
#include "NodeConfig.hpp"
#include "PeerManager.hpp"
#include "SyncState.hpp"
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

    // Load config.json from blockchain data directory
    NodeConfig node_config;
    try {
        node_config = NodeConfig::load(blockchainDir / "config.json");
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to load config: " + std::string(e.what()));
        return 1;
    }

    // Resolve TLS paths relative to blockchainDir if not absolute
    auto resolvePath = [&](const std::string &p) -> std::string {
        std::filesystem::path fp(p);
        if (fp.is_absolute()) return p;
        return (blockchainDir / fp).string();
    };
    std::string cert_file = resolvePath(node_config.tls.cert_file);
    std::string key_file = resolvePath(node_config.tls.key_file);
    std::string ca_file = node_config.tls.ca_file.empty() ? "" : resolvePath(node_config.tls.ca_file);

    int timeout_seconds = static_cast<int>(node_config.network.timeout_seconds);

    Blockchain<Chunk> bc(blockchainDir, node_config.to_consensus_config());
    bc.loadChunk(0);
    bc.loadKeys();
    bc.dumpBlocks();

    SyncStatus sync_status;

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

    // Create PeerManager
    PeerManager peer_manager(io_context, p2p_ssl_context, node_config.to_peer_config(), blockchainDir, bc, sync_status, node_config.network.p2p_port);

    tcp::acceptor rpc_acceptor(io_context);
    tcp::endpoint endpoint(tcp::v6(), node_config.network.rpc_port);
    rpc_acceptor.open(endpoint.protocol());
    rpc_acceptor.set_option(tcp::acceptor::reuse_address(true));
    rpc_acceptor.set_option(boost::asio::ip::v6_only(false));
    rpc_acceptor.bind(endpoint);
    rpc_acceptor.listen();

    Server<RpcServer, tcp::acceptor> rpc(io_context, rpc_ssl_context, rpc_acceptor, bc);
    rpc.set_timeout(std::chrono::seconds(timeout_seconds));
    rpc.set_peer_manager(&peer_manager);
    rpc.start_accept();

    tcp::acceptor p2p_acceptor(io_context);
    tcp::endpoint p2p_endpoint(tcp::v6(), node_config.network.p2p_port);
    p2p_acceptor.open(p2p_endpoint.protocol());
    p2p_acceptor.set_option(tcp::acceptor::reuse_address(true));
    p2p_acceptor.set_option(boost::asio::ip::v6_only(false));
    p2p_acceptor.bind(p2p_endpoint);
    p2p_acceptor.listen();

    Server<PeerServer, tcp::acceptor> node_server(io_context, p2p_ssl_context, p2p_acceptor, bc);
    node_server.set_timeout(std::chrono::seconds(timeout_seconds));
    node_server.set_peer_manager(&peer_manager);
    node_server.start_accept();

    // Start peer manager (connects to seeds, starts exchange timer)
    peer_manager.start();

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        logMessage("INFO", "Shutting down...");
        peer_manager.save_peers();
        bc.saveChunk(0);
        bc.saveKeys();
        io_context.stop();
    });

    io_context.run();
  
    return 0;
}
