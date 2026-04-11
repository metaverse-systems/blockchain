#pragma once

#include <catch2/catch_all.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <filesystem>
#include <fstream>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <memory>
#include <atomic>

#include "../src/Block.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include "../src/BlockPropagation.hpp"
#include "../src/ConsensusConfig.hpp"
#include "../src/NodeConfig.hpp"
#include "../src/PeerManager.hpp"
#include "../src/SyncState.hpp"
#include "../src/json.hpp"
#include "../src/network/Server.hpp"
#include "../src/network/RpcServer.hpp"
#include "../src/network/PeerServer.hpp"

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/err.h>

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

// T003: Self-signed certificate generation
struct TlsCertPair {
    std::string cert_path;
    std::string key_path;
};

inline TlsCertPair generate_self_signed_cert(const std::filesystem::path &dir) {
    TlsCertPair result;
    result.cert_path = (dir / "cert.pem").string();
    result.key_path = (dir / "key.pem").string();

    // Generate RSA-2048 key
    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    if (!kctx) throw std::runtime_error("EVP_PKEY_CTX_new_id failed");
    if (EVP_PKEY_keygen_init(kctx) <= 0) {
        EVP_PKEY_CTX_free(kctx);
        throw std::runtime_error("EVP_PKEY_keygen_init failed");
    }
    if (EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 2048) <= 0) {
        EVP_PKEY_CTX_free(kctx);
        throw std::runtime_error("EVP_PKEY_CTX_set_rsa_keygen_bits failed");
    }
    if (EVP_PKEY_keygen(kctx, &pkey) <= 0) {
        EVP_PKEY_CTX_free(kctx);
        throw std::runtime_error("EVP_PKEY_keygen failed");
    }
    EVP_PKEY_CTX_free(kctx);

    // Create X509 certificate
    X509 *cert = X509_new();
    if (!cert) {
        EVP_PKEY_free(pkey);
        throw std::runtime_error("X509_new failed");
    }

    X509_set_version(cert, 2); // V3
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600);
    X509_set_pubkey(cert, pkey);

    X509_NAME *name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char *>("localhost"), -1, -1, 0);
    X509_set_issuer_name(cert, name);

    if (X509_sign(cert, pkey, EVP_sha256()) <= 0) {
        X509_free(cert);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("X509_sign failed");
    }

    // Write certificate PEM
    FILE *cert_fp = fopen(result.cert_path.c_str(), "w");
    if (!cert_fp) {
        X509_free(cert);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("Cannot open cert file for writing");
    }
    PEM_write_X509(cert_fp, cert);
    fclose(cert_fp);

    // Write private key PEM
    FILE *key_fp = fopen(result.key_path.c_str(), "w");
    if (!key_fp) {
        X509_free(cert);
        EVP_PKEY_free(pkey);
        throw std::runtime_error("Cannot open key file for writing");
    }
    PEM_write_PrivateKey(key_fp, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(key_fp);

    X509_free(cert);
    EVP_PKEY_free(pkey);

    return result;
}

// T004: NodeInstance — manages a single in-process blockchain node
struct NodeInstance {
    std::filesystem::path data_dir;
    TlsCertPair tls;
    boost::asio::io_context io_context;

    // SSL contexts
    std::unique_ptr<ssl::context> rpc_ssl_ctx;
    std::unique_ptr<ssl::context> p2p_ssl_ctx;

    // Acceptors
    std::unique_ptr<tcp::acceptor> rpc_acceptor;
    std::unique_ptr<tcp::acceptor> p2p_acceptor;

    // Core components
    std::unique_ptr<Blockchain<Chunk>> blockchain;
    SyncStatus sync_status;
    std::unique_ptr<PeerManager> peer_manager;
    std::unique_ptr<BlockPropagation> block_propagation;

    // Servers
    std::unique_ptr<Server<RpcServer, tcp::acceptor>> rpc_server;
    std::unique_ptr<Server<PeerServer, tcp::acceptor>> p2p_server;

    // Thread
    std::thread io_thread;

    // Ports (set after bind)
    uint16_t rpc_port = 0;
    uint16_t p2p_port = 0;

    // Configuration
    std::vector<std::string> allowed_streams;
    std::vector<PeerAddress> seed_nodes;

    NodeInstance(const std::filesystem::path &dir,
                 const std::vector<std::string> &streams = {},
                 const std::vector<PeerAddress> &seeds = {},
                 const TlsCertPair *shared_tls = nullptr)
        : data_dir(dir), allowed_streams(streams), seed_nodes(seeds)
    {
        std::filesystem::create_directories(data_dir);

        // Use shared TLS certs if provided, otherwise generate per-node
        if (shared_tls) {
            tls = *shared_tls;
        } else {
            tls = generate_self_signed_cert(data_dir);
        }

        // Create consensus config with difficulty=0 for fast mining
        ConsensusConfig consensus_cfg;
        consensus_cfg.initialDifficulty = 0;
        consensus_cfg.minDifficulty = 0;
        consensus_cfg.miningTimeout = 60;

        // Create blockchain
        blockchain = std::make_unique<Blockchain<Chunk>>(data_dir, consensus_cfg);

        // Setup RPC SSL context (server-only TLS)
        rpc_ssl_ctx = std::make_unique<ssl::context>(ssl::context::tlsv12);
        rpc_ssl_ctx->set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::single_dh_use);
        rpc_ssl_ctx->use_certificate_chain_file(tls.cert_path);
        rpc_ssl_ctx->use_private_key_file(tls.key_path, ssl::context::pem);

        // Setup P2P SSL context (mutual TLS with self-signed cert as CA)
        p2p_ssl_ctx = std::make_unique<ssl::context>(ssl::context::tlsv12);
        p2p_ssl_ctx->set_options(ssl::context::default_workarounds | ssl::context::no_sslv2 | ssl::context::single_dh_use);
        p2p_ssl_ctx->use_certificate_chain_file(tls.cert_path);
        p2p_ssl_ctx->use_private_key_file(tls.key_path, ssl::context::pem);
        p2p_ssl_ctx->set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert);
        p2p_ssl_ctx->load_verify_file(tls.cert_path);

        // Bind RPC acceptor to port 0
        rpc_acceptor = std::make_unique<tcp::acceptor>(io_context);
        tcp::endpoint rpc_ep(tcp::v6(), 0);
        rpc_acceptor->open(rpc_ep.protocol());
        rpc_acceptor->set_option(tcp::acceptor::reuse_address(true));
        rpc_acceptor->set_option(boost::asio::ip::v6_only(false));
        rpc_acceptor->bind(rpc_ep);
        rpc_acceptor->listen();
        rpc_port = rpc_acceptor->local_endpoint().port();

        // Bind P2P acceptor to port 0
        p2p_acceptor = std::make_unique<tcp::acceptor>(io_context);
        tcp::endpoint p2p_ep(tcp::v6(), 0);
        p2p_acceptor->open(p2p_ep.protocol());
        p2p_acceptor->set_option(tcp::acceptor::reuse_address(true));
        p2p_acceptor->set_option(boost::asio::ip::v6_only(false));
        p2p_acceptor->bind(p2p_ep);
        p2p_acceptor->listen();
        p2p_port = p2p_acceptor->local_endpoint().port();

        // Create PeerConfig
        PeerConfig peer_cfg;
        peer_cfg.seed_nodes = seed_nodes;
        peer_cfg.discovery_enabled = !seed_nodes.empty();
        peer_cfg.max_outbound = 8;
        peer_cfg.max_inbound = 8;

        // Create PeerManager
        peer_manager = std::make_unique<PeerManager>(
            io_context, *p2p_ssl_ctx, peer_cfg, data_dir, *blockchain, sync_status, p2p_port);

        // Create BlockPropagation
        block_propagation = std::make_unique<BlockPropagation>(
            *blockchain, sync_status,
            [this](const Block &block, const std::string &exclude_key) {
                peer_manager->relay_block(block, exclude_key);
            });
        block_propagation->set_peer_manager(peer_manager.get());
        peer_manager->set_block_propagation(block_propagation.get());

        // Create RPC server
        rpc_server = std::make_unique<Server<RpcServer, tcp::acceptor>>(
            io_context, *rpc_ssl_ctx, *rpc_acceptor, *blockchain);
        rpc_server->set_timeout(std::chrono::seconds(30));
        rpc_server->set_peer_manager(peer_manager.get());
        rpc_server->set_allowed_streams(allowed_streams);
        rpc_server->start_accept();

        // Create P2P server
        p2p_server = std::make_unique<Server<PeerServer, tcp::acceptor>>(
            io_context, *p2p_ssl_ctx, *p2p_acceptor, *blockchain);
        p2p_server->set_timeout(std::chrono::seconds(30));
        p2p_server->set_peer_manager(peer_manager.get());
        p2p_server->set_block_propagation(block_propagation.get());
        p2p_server->start_accept();

        // Start peer manager (connects to seeds)
        peer_manager->start();

        // Run io_context on a background thread
        io_thread = std::thread([this] { io_context.run(); });

        // Wait for readiness (acceptors are already listening after listen() call)
        // Small delay to ensure io_context.run() has started processing
        wait_for_ready();
    }

    void wait_for_ready(int timeout_ms = 10000) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(timeout_ms)) {
            // Try connecting to the RPC port to verify it's accepting
            try {
                boost::asio::io_context test_io;
                tcp::socket test_sock(test_io);
                tcp::endpoint ep(boost::asio::ip::address_v4::loopback(), rpc_port);
                test_sock.connect(ep);
                test_sock.close();
                return;
            } catch (...) {
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
        throw std::runtime_error("NodeInstance failed to become ready within timeout");
    }

    void stop() {
        io_context.stop();
        if (io_thread.joinable()) io_thread.join();
    }

    // Manually connect this node to another node's P2P port
    void connect_to_peer(const std::string &host, uint16_t port) {
        peer_manager->connect_to(host, port);
    }

    ~NodeInstance() {
        stop();
    }

    // Non-copyable, non-movable (owns threads and io_context)
    NodeInstance(const NodeInstance&) = delete;
    NodeInstance& operator=(const NodeInstance&) = delete;
    NodeInstance(NodeInstance&&) = delete;
    NodeInstance& operator=(NodeInstance&&) = delete;
};

// T005: RpcTestClient — synchronous TLS client for JSON-RPC
class RpcTestClient {
public:
    RpcTestClient(const std::string &host, uint16_t port)
        : host_(host), port_(port),
          ssl_ctx_(ssl::context::tlsv12),
          socket_(io_ctx_, ssl_ctx_)
    {
        ssl_ctx_.set_verify_mode(ssl::verify_none);
        socket_ = ssl::stream<tcp::socket>(io_ctx_, ssl_ctx_);
    }

    void connect() {
        tcp::resolver resolver(io_ctx_);
        auto endpoints = resolver.resolve(host_, std::to_string(port_));
        boost::asio::connect(socket_.lowest_layer(), endpoints);
        socket_.handshake(ssl::stream_base::client);
    }

    nlohmann::json call(const std::string &method, const nlohmann::json &params = nlohmann::json::object()) {
        nlohmann::json request;
        request["jsonrpc"] = "2.0";
        request["id"] = std::to_string(++request_id_);
        request["method"] = method;
        request["params"] = params;

        std::string msg = request.dump() + "\n";
        boost::asio::write(socket_, boost::asio::buffer(msg));

        // Read response with 10-second deadline
        boost::asio::streambuf response_buf;
        boost::asio::steady_timer deadline(io_ctx_);
        deadline.expires_after(std::chrono::seconds(10));

        std::atomic<bool> got_response{false};
        boost::system::error_code read_ec;

        boost::asio::async_read_until(socket_, response_buf, '\n',
            [&](const boost::system::error_code &ec, std::size_t) {
                read_ec = ec;
                got_response = true;
                deadline.cancel();
            });

        deadline.async_wait([&](const boost::system::error_code &ec) {
            if (!ec) {
                socket_.lowest_layer().cancel();
            }
        });

        io_ctx_.restart();
        io_ctx_.run();

        if (read_ec) {
            throw std::runtime_error("RPC read failed: " + read_ec.message());
        }

        std::istream stream(&response_buf);
        std::string line;
        std::getline(stream, line);
        return nlohmann::json::parse(line);
    }

    // Send raw text (for malformed JSON tests)
    nlohmann::json send_raw(const std::string &raw) {
        boost::asio::write(socket_, boost::asio::buffer(raw));

        boost::asio::streambuf response_buf;
        boost::asio::steady_timer deadline(io_ctx_);
        deadline.expires_after(std::chrono::seconds(10));

        boost::system::error_code read_ec;

        boost::asio::async_read_until(socket_, response_buf, '\n',
            [&](const boost::system::error_code &ec, std::size_t) {
                read_ec = ec;
                deadline.cancel();
            });

        deadline.async_wait([&](const boost::system::error_code &ec) {
            if (!ec) {
                socket_.lowest_layer().cancel();
            }
        });

        io_ctx_.restart();
        io_ctx_.run();

        if (read_ec) {
            throw std::runtime_error("RPC read failed: " + read_ec.message());
        }

        std::istream stream(&response_buf);
        std::string line;
        std::getline(stream, line);
        return nlohmann::json::parse(line);
    }

private:
    std::string host_;
    uint16_t port_;
    boost::asio::io_context io_ctx_;
    ssl::context ssl_ctx_;
    ssl::stream<tcp::socket> socket_;
    int request_id_ = 0;
};

// T006: IntegrationTestFixture — manages multiple NodeInstances
class IntegrationTestFixture {
public:
    IntegrationTestFixture()
        : base_dir_(std::filesystem::temp_directory_path() /
                    ("integ_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())))
    {
        std::filesystem::create_directories(base_dir_);
        // Generate a shared TLS cert for all nodes (mutual TLS requires shared CA)
        shared_tls_ = generate_self_signed_cert(base_dir_);
    }

    ~IntegrationTestFixture() {
        // Stop all nodes first
        for (auto &node : nodes_) {
            node->stop();
        }
        nodes_.clear();
        // Clean up temp directory
        std::error_code ec;
        std::filesystem::remove_all(base_dir_, ec);
    }

    NodeInstance* create_node(const std::vector<std::string> &allowed_streams = {},
                              const std::vector<PeerAddress> &seed_nodes = {}) {
        auto node_dir = base_dir_ / ("node_" + std::to_string(node_counter_++));
        nodes_.push_back(std::make_unique<NodeInstance>(node_dir, allowed_streams, seed_nodes, &shared_tls_));
        return nodes_.back().get();
    }

    std::unique_ptr<RpcTestClient> create_rpc_client(const NodeInstance *node) {
        auto client = std::make_unique<RpcTestClient>("127.0.0.1", node->rpc_port);
        client->connect();
        return client;
    }

    const std::filesystem::path& base_dir() const { return base_dir_; }

    // Non-copyable, non-movable
    IntegrationTestFixture(const IntegrationTestFixture&) = delete;
    IntegrationTestFixture& operator=(const IntegrationTestFixture&) = delete;

private:
    std::filesystem::path base_dir_;
    TlsCertPair shared_tls_;
    std::vector<std::unique_ptr<NodeInstance>> nodes_;
    int node_counter_ = 0;
};
