#pragma once

#include <string>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include "json.hpp"
#include "PeerConfig.hpp"
#include "ConsensusConfig.hpp"
#include "utils.hpp"

class NodeConfig {
public:
    // TLS settings
    struct TlsConfig {
        std::string cert_file = "cert.pem";
        std::string key_file = "key.pem";
        std::string ca_file;
    } tls;

    // Network settings
    struct NetworkConfig {
        uint16_t rpc_port = 12345;
        uint16_t p2p_port = 12346;
        uint32_t timeout_seconds = 30;
        std::string log_level = "info";
    } network;

    // Consensus settings
    ConsensusConfig consensus;

    // Peer discovery settings
    PeerConfig peers;

    // Persistence settings
    struct PersistenceConfig {
        uint32_t save_interval_seconds = 300;
        bool fast_startup = false;
    } persistence;

    // Streams settings
    struct StreamsConfig {
        std::vector<std::string> allowed_streams;
    } streams;

    // Load config from a JSON file; throws on parse/validation errors
    static NodeConfig load(const std::filesystem::path &config_path);

    // Validate the loaded configuration; throws std::invalid_argument on failure
    // blockchain_dir is used to resolve relative TLS paths for file-existence checks
    void validate(const std::filesystem::path &blockchain_dir = {}) const;

    // Generate a default config.json at the given path
    static void generate_default(const std::filesystem::path &config_path);

    // Generate a config.README documentation file at the given path
    static void generate_readme(const std::filesystem::path &readme_path);

    // Build a ConsensusConfig from the loaded values
    ConsensusConfig to_consensus_config() const { return consensus; }

    // Build a PeerConfig from the loaded values
    PeerConfig to_peer_config() const { return peers; }

private:
    static nlohmann::json default_json();
};
