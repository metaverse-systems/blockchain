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
    } network;

    // Consensus settings
    ConsensusConfig consensus;

    // Peer discovery settings
    PeerConfig peers;

    // Streams settings
    struct StreamsConfig {
        std::vector<std::string> allowed_streams;
    } streams;

    // Load config from a JSON file; throws on parse/validation errors
    static NodeConfig load(const std::filesystem::path &config_path);

    // Validate the loaded configuration; throws std::invalid_argument on failure
    void validate() const;

    // Generate a default config.json at the given path
    static void generate_default(const std::filesystem::path &config_path);

    // Build a ConsensusConfig from the loaded values
    ConsensusConfig to_consensus_config() const { return consensus; }

    // Build a PeerConfig from the loaded values
    PeerConfig to_peer_config() const { return peers; }

private:
    static nlohmann::json default_json();
};
