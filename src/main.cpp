#include <iostream>
#include <boost/asio.hpp>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include "Block.hpp"
#include "Blockchain.hpp"
#include "BlockPropagation.hpp"
#include "ChainService.hpp"
#include "CliParser.hpp"
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
    CliOptions cli;
    try {
        cli = CliParser::parse(argc, argv);
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << CliParser::usage_string() << "\n";
        return 1;
    }

    if (cli.show_help) {
        std::cout << CliParser::usage_string() << "\n";
        return 0;
    }
    if (cli.show_version) {
        std::cout << CliParser::version_string() << "\n";
        return 0;
    }

    if (cli.blockchain_dir.empty()) {
        std::cerr << CliParser::usage_string() << "\n";
        return 1;
    }

    std::filesystem::path blockchainDir(cli.blockchain_dir);

    if (!std::filesystem::exists(blockchainDir)) {
        std::cerr << "Error: blockchain directory does not exist: " << blockchainDir.string() << "\n";
        return 1;
    }

    // Handle --generate-config before loading any config
    if (cli.generate_config) {
        auto cfg_path = blockchainDir / "config.json";
        if (std::filesystem::exists(cfg_path)) {
            std::cerr << "Error: config.json already exists at " << cfg_path.string() << "\n";
            return 1;
        }
        try {
            NodeConfig::generate_default(cfg_path);
            NodeConfig::generate_readme(blockchainDir / "config.README");
            std::cout << "Generated " << cfg_path.string() << "\n";
            std::cout << "Generated " << (blockchainDir / "config.README").string() << "\n";
        } catch (const std::exception &e) {
            std::cerr << "Error: " << e.what() << "\n";
            return 1;
        }
        return 0;
    }

    // Determine config path (CLI --config or default)
    std::filesystem::path config_path = cli.config_path
        ? std::filesystem::path(*cli.config_path)
        : blockchainDir / "config.json";

    // Load config.json
    NodeConfig node_config;
    try {
        node_config = NodeConfig::load(config_path);
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to load config: " + std::string(e.what()));
        return 1;
    }

    // Apply CLI overrides onto loaded config
    if (cli.rpc_port) node_config.network.rpc_port = *cli.rpc_port;
    if (cli.p2p_port) node_config.network.p2p_port = *cli.p2p_port;
    if (cli.log_level) node_config.network.log_level = *cli.log_level;
    for (const auto &seed : cli.seed_nodes) {
        try {
            auto [host, port] = parsePeerKey(seed);
            PeerAddress addr;
            addr.host = host;
            addr.port = port;
            node_config.peers.seed_nodes.push_back(addr);
        } catch (const std::invalid_argument &e) {
            std::cerr << "Invalid seed node '" << seed << "': " << e.what() << "\n";
            return 1;
        }
    }

    // Validate merged config
    try {
        node_config.validate(blockchainDir);
    } catch (const std::invalid_argument &e) {
        std::cerr << e.what();
        return 1;
    }

    // Set global log level from final config
    setLogLevel(parseLogLevel(node_config.network.log_level));

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

    // Recovery: discover chunk files on disk, load active chunk and indexes
    size_t discoveredChunks = bc.discoverChunks();
    if (discoveredChunks > 0) {
        bc.recoverChain(node_config.persistence.fast_startup);
    } else {
        bc.loadChunk(0);
        bc.loadKeys();
        bc.loadStreams();
        bc.loadStreamIndex();
    }
    bc.dumpBlocks();

    SyncStatus sync_status;

    ChainService chain_service(bc);

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
    PeerManager peer_manager(io_context, p2p_ssl_context, node_config.to_peer_config(), blockchainDir, bc, chain_service, sync_status, node_config.network.p2p_port);

    // Create BlockPropagation with relay callback
    BlockPropagation block_propagation(bc, chain_service, sync_status,
        [&peer_manager](const Block &block, const std::string &exclude_key) {
            peer_manager.relay_block(block, exclude_key);
        });
    block_propagation.set_peer_manager(&peer_manager);
    peer_manager.set_block_propagation(&block_propagation);

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
    rpc.set_allowed_streams(node_config.streams.allowed_streams);
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
    node_server.set_block_propagation(&block_propagation);
    node_server.start_accept();

    // Start peer manager (connects to seeds, starts exchange timer)
    peer_manager.start();

    // Start periodic save timer if configured
    bc.setSaveIntervalSeconds(node_config.persistence.save_interval_seconds);
    bc.startPeriodicSave(io_context);

    boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code&, int) {
        logMessage("INFO", "Shutting down...");
        bc.setShuttingDown();
        bc.stopPeriodicSave();
        peer_manager.save_peers();
        bc.saveAllChunks();
        io_context.stop();
    });

    // All async handlers in this application assume single-threaded
    // io_context execution. Do NOT add threads or call io_context.run()
    // from multiple threads without first adding strand/mutex protection
    // to BlockPropagation, PeerManager, Blockchain, and RpcServer.
    static std::atomic<int> run_count{0};
    if (++run_count != 1) {
        std::cerr << "FATAL: io_context::run() must be called from exactly one thread" << std::endl;
        std::abort();
    }
    io_context.run();
  
    return 0;
}
