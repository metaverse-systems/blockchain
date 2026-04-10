#include <catch2/catch_all.hpp>
#include "../src/NodeConfig.hpp"
#include <fstream>
#include <filesystem>

static std::filesystem::path make_temp_dir() {
    auto p = std::filesystem::temp_directory_path() / ("nodeconfig_test_" + std::to_string(std::rand()));
    std::filesystem::create_directories(p);
    return p;
}

TEST_CASE("NodeConfig generates default config when missing", "[NodeConfig]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";

    NodeConfig cfg = NodeConfig::load(cfg_path);

    REQUIRE(std::filesystem::exists(cfg_path));
    REQUIRE(cfg.tls.cert_file == "cert.pem");
    REQUIRE(cfg.tls.key_file == "key.pem");
    REQUIRE(cfg.network.rpc_port == 12345);
    REQUIRE(cfg.network.p2p_port == 12346);
    REQUIRE(cfg.network.timeout_seconds == 30);
    REQUIRE(cfg.peers.max_outbound == 8);
    REQUIRE(cfg.peers.max_inbound == 32);
    REQUIRE(cfg.peers.discovery_enabled == true);
    REQUIRE(cfg.peers.max_stored_peers == 256);

    std::filesystem::remove_all(dir);
}

TEST_CASE("NodeConfig loads custom values from JSON", "[NodeConfig]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";

    nlohmann::json j = {
        {"tls", {{"cert_file", "my_cert.pem"}, {"key_file", "my_key.pem"}, {"ca_file", "my_ca.pem"}}},
        {"network", {{"rpc_port", 9000}, {"p2p_port", 9001}, {"timeout_seconds", 60}}},
        {"consensus", {{"target_block_interval", 20}}},
        {"peers", {
            {"seed_nodes", {{{"host", "10.0.0.1"}, {"port", 9001}}}},
            {"max_outbound", 4},
            {"max_inbound", 16},
            {"exchange_interval_seconds", 60},
            {"discovery_enabled", false},
            {"max_stored_peers", 128},
            {"reconnect_base_delay_seconds", 10},
            {"reconnect_max_delay_seconds", 600},
            {"ban_threshold_errors", 5},
            {"ban_duration_seconds", 7200}
        }}
    };
    std::ofstream(cfg_path) << j.dump(2);

    NodeConfig cfg = NodeConfig::load(cfg_path);

    REQUIRE(cfg.tls.cert_file == "my_cert.pem");
    REQUIRE(cfg.tls.ca_file == "my_ca.pem");
    REQUIRE(cfg.network.rpc_port == 9000);
    REQUIRE(cfg.network.p2p_port == 9001);
    REQUIRE(cfg.network.timeout_seconds == 60);
    REQUIRE(cfg.consensus.targetBlockInterval == 20);
    REQUIRE(cfg.peers.seed_nodes.size() == 1);
    REQUIRE(cfg.peers.seed_nodes[0].host == "10.0.0.1");
    REQUIRE(cfg.peers.seed_nodes[0].port == 9001);
    REQUIRE(cfg.peers.max_outbound == 4);
    REQUIRE(cfg.peers.discovery_enabled == false);
    REQUIRE(cfg.peers.ban_threshold_errors == 5);

    std::filesystem::remove_all(dir);
}

TEST_CASE("NodeConfig validation rejects invalid config", "[NodeConfig]") {
    SECTION("Empty cert_file") {
        NodeConfig cfg;
        cfg.tls.cert_file = "";
        REQUIRE_THROWS_AS(cfg.validate(), std::invalid_argument);
    }

    SECTION("Empty key_file") {
        NodeConfig cfg;
        cfg.tls.key_file = "";
        REQUIRE_THROWS_AS(cfg.validate(), std::invalid_argument);
    }

    SECTION("Same rpc and p2p port") {
        NodeConfig cfg;
        cfg.network.rpc_port = 5000;
        cfg.network.p2p_port = 5000;
        REQUIRE_THROWS_AS(cfg.validate(), std::invalid_argument);
    }

    SECTION("Zero rpc_port") {
        NodeConfig cfg;
        cfg.network.rpc_port = 0;
        REQUIRE_THROWS_AS(cfg.validate(), std::invalid_argument);
    }

    SECTION("exchange_interval_seconds too low") {
        NodeConfig cfg;
        cfg.peers.exchange_interval_seconds = 2;
        REQUIRE_THROWS_AS(cfg.validate(), std::invalid_argument);
    }

    SECTION("reconnect_max < reconnect_base") {
        NodeConfig cfg;
        cfg.peers.reconnect_base_delay_seconds = 100;
        cfg.peers.reconnect_max_delay_seconds = 50;
        REQUIRE_THROWS_AS(cfg.validate(), std::invalid_argument);
    }

    SECTION("ban_threshold_errors zero") {
        NodeConfig cfg;
        cfg.peers.ban_threshold_errors = 0;
        REQUIRE_THROWS_AS(cfg.validate(), std::invalid_argument);
    }
}

TEST_CASE("NodeConfig merges missing keys with defaults", "[NodeConfig]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";

    // Write a minimal config with only TLS and network — peers section missing
    nlohmann::json j = {
        {"tls", {{"cert_file", "c.pem"}, {"key_file", "k.pem"}}},
        {"network", {{"rpc_port", 8000}, {"p2p_port", 8001}}}
    };
    std::ofstream(cfg_path) << j.dump(2);

    NodeConfig cfg = NodeConfig::load(cfg_path);

    REQUIRE(cfg.tls.cert_file == "c.pem");
    REQUIRE(cfg.network.rpc_port == 8000);
    // Missing peers section should get defaults
    REQUIRE(cfg.peers.max_outbound == 8);
    REQUIRE(cfg.peers.discovery_enabled == true);

    std::filesystem::remove_all(dir);
}
