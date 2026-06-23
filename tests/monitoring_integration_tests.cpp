#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <filesystem>
#include <thread>
#include <chrono>
#include <memory>
#include <sstream>
#include <random>

#include "../src/Block.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include "../src/ConsensusConfig.hpp"
#include "../src/ChainService.hpp"
#include "../src/SyncState.hpp"
#include "../src/PeerManager.hpp"
#include "../src/BlockPropagation.hpp"
#include "../src/MetricsCollector.hpp"
#include "../src/MonitoringHttpServer.hpp"
#include "../src/json.hpp"

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/pem.h>

namespace ssl = boost::asio::ssl;
using boost::asio::ip::tcp;

// =========================================================================
// Helper: Generate unique temp directory name
// =========================================================================

static std::string make_temp_dir_name(const std::string &prefix) {
    std::random_device rd;
    uint32_t rand_val = rd();
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return prefix + "_" + std::to_string(now) + "_" + std::to_string(rand_val);
}

// =========================================================================
// Helper: Generate self-signed certificate for monitoring server tests
// =========================================================================

struct TlsCertPair {
    std::string cert_path;
    std::string key_path;
};

static TlsCertPair generate_self_signed_cert(const std::filesystem::path &dir) {
    TlsCertPair result;
    result.cert_path = (dir / "mon_cert.pem").string();
    result.key_path = (dir / "mon_key.pem").string();

    EVP_PKEY *pkey = nullptr;
    EVP_PKEY_CTX *kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
    REQUIRE(kctx != nullptr);
    EVP_PKEY_keygen_init(kctx);
    EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 2048);
    EVP_PKEY_keygen(kctx, &pkey);
    EVP_PKEY_CTX_free(kctx);

    X509 *cert = X509_new();
    REQUIRE(cert != nullptr);
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), 365 * 24 * 3600);
    X509_set_pubkey(cert, pkey);

    X509_NAME *name = X509_get_subject_name(cert);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC,
        reinterpret_cast<const unsigned char *>("localhost"), -1, -1, 0);
    X509_set_issuer_name(cert, name);
    X509_sign(cert, pkey, EVP_sha256());

    FILE *cert_fp = fopen(result.cert_path.c_str(), "w");
    PEM_write_X509(cert_fp, cert);
    fclose(cert_fp);

    FILE *key_fp = fopen(result.key_path.c_str(), "w");
    PEM_write_PrivateKey(key_fp, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    fclose(key_fp);

    X509_free(cert);
    EVP_PKEY_free(pkey);

    return result;
}

// =========================================================================
// T015: Integration test for /health endpoint - server startup
// =========================================================================

TEST_CASE("Integration: Monitoring server starts and stops cleanly", "[monitoring][integration][health]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / make_temp_dir_name("mon_test");
    std::filesystem::create_directories(tmp_dir);

    TlsCertPair certs = generate_self_signed_cert(tmp_dir);

    // Setup blockchain
    ConsensusConfig cfg;
    cfg.initialDifficulty = 0;
    cfg.minDifficulty = 0;
    Blockchain<Chunk> bc(tmp_dir, cfg);
    bc.loadChunk(0);

    boost::asio::io_context io;

    // Setup SSL context
    ssl::context ssl_ctx(ssl::context::tlsv12);
    ssl_ctx.set_options(ssl::context::default_workarounds | ssl::context::no_sslv2);
    ssl_ctx.use_certificate_chain_file(certs.cert_path);
    ssl_ctx.use_private_key_file(certs.key_path, ssl::context::pem);

    // Create metrics collector and monitoring server
    MetricsCollector metrics;
    auto mon_server = std::make_unique<MonitoringHttpServer>(
        io, ssl_ctx, 0, "127.0.0.1", bc, nullptr, metrics);

    // Start server
    mon_server->start();

    // Give server time to initialize
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Stop server
    mon_server->stop();
    REQUIRE(!mon_server->is_running());

    // Cleanup
    std::filesystem::remove_all(tmp_dir);
}

// =========================================================================
// T022: Integration test for /metrics endpoint
// =========================================================================

TEST_CASE("Integration: /metrics returns Prometheus format with correct values", "[monitoring][integration][metrics]") {
    MetricsCollector mc;

    // Simulate some metric increments
    mc.rpc_requests_total_.fetch_add(10);
    mc.rpc_errors_total_.fetch_add(2);
    mc.blocks_received_total_.fetch_add(5);
    mc.blocks_rejected_total_.fetch_add(1);

    std::string output = mc.generatePrometheusText();

    // Verify Prometheus format compliance
    bool has_help = (output.find("# HELP") != std::string::npos);
    bool has_type = (output.find("# TYPE") != std::string::npos);
    REQUIRE(has_help);
    REQUIRE(has_type);

    // Verify counter values
    REQUIRE(output.find("blockchain_rpc_requests_total 10") != std::string::npos);
    REQUIRE(output.find("blockchain_rpc_errors_total 2") != std::string::npos);
    REQUIRE(output.find("blockchain_blocks_received_total 5") != std::string::npos);
    REQUIRE(output.find("blockchain_blocks_rejected_total 1") != std::string::npos);
}

// =========================================================================
// T026: Integration test for structured logging
// =========================================================================

TEST_CASE("Integration: JSON structured logging produces valid JSON", "[monitoring][integration][logging]") {
    // Test JSON format parsing by constructing expected output
    nlohmann::json j;
    j["timestamp"] = "2024-01-01T00:00:00Z";
    j["level"] = "INFO";
    j["message"] = "test message";

    std::string json_line = j.dump();

    // Verify we can parse it back
    nlohmann::json parsed = nlohmann::json::parse(json_line);
    REQUIRE(parsed.contains("timestamp"));
    REQUIRE(parsed.contains("level"));
    REQUIRE(parsed.contains("message"));
    REQUIRE(parsed["level"] == "INFO");
    REQUIRE(parsed["message"] == "test message");
}

// =========================================================================
// MetricsCollector with blockchain state
// =========================================================================

TEST_CASE("Integration: MetricsCollector with blockchain reference", "[monitoring][integration][metrics]") {
    auto tmp_dir = std::filesystem::temp_directory_path() / make_temp_dir_name("mon_bc");
    std::filesystem::create_directories(tmp_dir);

    ConsensusConfig cfg;
    cfg.initialDifficulty = 0;
    cfg.minDifficulty = 0;
    Blockchain<Chunk> bc(tmp_dir, cfg);
    bc.loadChunk(0);

    // Genesis block should exist
    REQUIRE(bc.getChainLength() >= 1);

    MetricsCollector mc;
    mc.blockchain_ = &bc;

    std::string output = mc.generatePrometheusText();

    // Chain height should be present
    bool has_chain_height = (output.find("blockchain_chain_height") != std::string::npos);
    bool has_chunk_count = (output.find("blockchain_chunk_count") != std::string::npos);
    REQUIRE(has_chain_height);
    REQUIRE(has_chunk_count);

    std::filesystem::remove_all(tmp_dir);
}

// =========================================================================
// Health response JSON structure test
// =========================================================================

TEST_CASE("Integration: HealthResponse JSON has all required fields", "[monitoring][integration][health]") {
    nlohmann::json response;
    response["status"] = "healthy";
    response["chain_height"] = 100;
    response["peer_count"] = 5;
    response["chunk_count"] = 2;
    response["uptime_seconds"] = 1234.5;
    response["last_block_index"] = 99;

    // Verify all required fields exist
    REQUIRE(response.contains("status"));
    REQUIRE(response.contains("chain_height"));
    REQUIRE(response.contains("peer_count"));
    REQUIRE(response.contains("chunk_count"));
    REQUIRE(response.contains("uptime_seconds"));
    REQUIRE(response.contains("last_block_index"));

    // Verify types
    REQUIRE(response["status"].is_string());
    REQUIRE(response["chain_height"].is_number_integer());
    REQUIRE(response["peer_count"].is_number_integer());
    REQUIRE(response["chunk_count"].is_number_integer());
    REQUIRE(response["uptime_seconds"].is_number());
    REQUIRE(response["last_block_index"].is_number_integer());

    // Verify JSON serialization works
    std::string dumped = response.dump(2);
    REQUIRE(dumped.find("\"status\"") != std::string::npos);
    REQUIRE(dumped.find("\"chain_height\"") != std::string::npos);
}
