#include "NodeConfig.hpp"
#include <stdexcept>

nlohmann::json NodeConfig::default_json() {
    return {
        {"tls", {
            {"cert_file", "cert.pem"},
            {"key_file", "key.pem"},
            {"ca_file", ""}
        }},
        {"network", {
            {"rpc_port", 12345},
            {"p2p_port", 12346},
            {"timeout_seconds", 30}
        }},
        {"consensus", {
            {"target_block_interval", 10},
            {"adjustment_window", 10},
            {"max_adjustment_factor", 4.0},
            {"min_difficulty", 1},
            {"max_difficulty", 16},
            {"initial_difficulty", 1},
            {"mining_timeout", 30},
            {"max_future_timestamp", 120},
            {"max_reorg_depth", 100}
        }},
        {"peers", {
            {"seed_nodes", nlohmann::json::array()},
            {"max_outbound", 8},
            {"max_inbound", 32},
            {"exchange_interval_seconds", 30},
            {"discovery_enabled", true},
            {"max_stored_peers", 256},
            {"reconnect_base_delay_seconds", 5},
            {"reconnect_max_delay_seconds", 300},
            {"ban_threshold_errors", 10},
            {"ban_duration_seconds", 3600}
        }}
    };
}

void NodeConfig::generate_default(const std::filesystem::path &config_path) {
    nlohmann::json j = default_json();
    std::ofstream ofs(config_path);
    if (!ofs.is_open()) {
        throw std::runtime_error("Cannot create config file: " + config_path.string());
    }
    ofs << j.dump(2) << std::endl;
}

NodeConfig NodeConfig::load(const std::filesystem::path &config_path) {
    if (!std::filesystem::exists(config_path)) {
        logMessage("INFO", "No config.json found, generating default at " + config_path.string());
        generate_default(config_path);
    }

    std::ifstream ifs(config_path);
    if (!ifs.is_open()) {
        throw std::runtime_error("Cannot open config file: " + config_path.string());
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(ifs);
    } catch (const nlohmann::json::parse_error &e) {
        throw std::runtime_error("Failed to parse config.json: " + std::string(e.what()));
    }

    // Merge with defaults so missing keys get default values
    nlohmann::json defaults = default_json();
    for (auto &[key, val] : defaults.items()) {
        if (!j.contains(key)) {
            j[key] = val;
        } else if (val.is_object() && j[key].is_object()) {
            for (auto &[sub_key, sub_val] : val.items()) {
                if (!j[key].contains(sub_key)) {
                    j[key][sub_key] = sub_val;
                }
            }
        }
    }

    NodeConfig cfg;

    // TLS
    if (j.contains("tls")) {
        auto &t = j["tls"];
        if (t.contains("cert_file")) t["cert_file"].get_to(cfg.tls.cert_file);
        if (t.contains("key_file")) t["key_file"].get_to(cfg.tls.key_file);
        if (t.contains("ca_file")) t["ca_file"].get_to(cfg.tls.ca_file);
    }

    // Network
    if (j.contains("network")) {
        auto &n = j["network"];
        if (n.contains("rpc_port")) n["rpc_port"].get_to(cfg.network.rpc_port);
        if (n.contains("p2p_port")) n["p2p_port"].get_to(cfg.network.p2p_port);
        if (n.contains("timeout_seconds")) n["timeout_seconds"].get_to(cfg.network.timeout_seconds);
    }

    // Consensus
    if (j.contains("consensus")) {
        auto &c = j["consensus"];
        if (c.contains("target_block_interval")) cfg.consensus.targetBlockInterval = c["target_block_interval"].get<uint32_t>();
        if (c.contains("adjustment_window")) cfg.consensus.adjustmentWindow = c["adjustment_window"].get<uint32_t>();
        if (c.contains("max_adjustment_factor")) cfg.consensus.maxAdjustmentFactor = c["max_adjustment_factor"].get<double>();
        if (c.contains("min_difficulty")) cfg.consensus.minDifficulty = c["min_difficulty"].get<uint32_t>();
        if (c.contains("max_difficulty")) cfg.consensus.maxDifficulty = c["max_difficulty"].get<uint32_t>();
        if (c.contains("initial_difficulty")) cfg.consensus.initialDifficulty = c["initial_difficulty"].get<uint32_t>();
        if (c.contains("mining_timeout")) cfg.consensus.miningTimeout = c["mining_timeout"].get<uint32_t>();
        if (c.contains("max_future_timestamp")) cfg.consensus.maxFutureTimestamp = c["max_future_timestamp"].get<uint32_t>();
        if (c.contains("max_reorg_depth")) cfg.consensus.maxReorgDepth = c["max_reorg_depth"].get<uint32_t>();
    }

    // Peers
    if (j.contains("peers")) {
        auto &p = j["peers"];
        if (p.contains("seed_nodes")) cfg.peers.seed_nodes = p["seed_nodes"].get<std::vector<PeerAddress>>();
        if (p.contains("max_outbound")) p["max_outbound"].get_to(cfg.peers.max_outbound);
        if (p.contains("max_inbound")) p["max_inbound"].get_to(cfg.peers.max_inbound);
        if (p.contains("exchange_interval_seconds")) p["exchange_interval_seconds"].get_to(cfg.peers.exchange_interval_seconds);
        if (p.contains("discovery_enabled")) p["discovery_enabled"].get_to(cfg.peers.discovery_enabled);
        if (p.contains("max_stored_peers")) p["max_stored_peers"].get_to(cfg.peers.max_stored_peers);
        if (p.contains("reconnect_base_delay_seconds")) p["reconnect_base_delay_seconds"].get_to(cfg.peers.reconnect_base_delay_seconds);
        if (p.contains("reconnect_max_delay_seconds")) p["reconnect_max_delay_seconds"].get_to(cfg.peers.reconnect_max_delay_seconds);
        if (p.contains("ban_threshold_errors")) p["ban_threshold_errors"].get_to(cfg.peers.ban_threshold_errors);
        if (p.contains("ban_duration_seconds")) p["ban_duration_seconds"].get_to(cfg.peers.ban_duration_seconds);
    }

    cfg.validate();
    return cfg;
}

void NodeConfig::validate() const {
    if (tls.cert_file.empty()) {
        throw std::invalid_argument("tls.cert_file must be non-empty");
    }
    if (tls.key_file.empty()) {
        throw std::invalid_argument("tls.key_file must be non-empty");
    }
    if (network.rpc_port == 0) {
        throw std::invalid_argument("network.rpc_port must be > 0");
    }
    if (network.p2p_port == 0) {
        throw std::invalid_argument("network.p2p_port must be > 0");
    }
    if (network.rpc_port == network.p2p_port) {
        throw std::invalid_argument("network.rpc_port and network.p2p_port must differ");
    }
    if (peers.max_outbound == 0) {
        throw std::invalid_argument("peers.max_outbound must be > 0");
    }
    if (peers.max_inbound == 0) {
        throw std::invalid_argument("peers.max_inbound must be > 0");
    }
    if (peers.exchange_interval_seconds < 5) {
        throw std::invalid_argument("peers.exchange_interval_seconds must be >= 5");
    }
    if (peers.reconnect_base_delay_seconds < 1) {
        throw std::invalid_argument("peers.reconnect_base_delay_seconds must be >= 1");
    }
    if (peers.reconnect_max_delay_seconds < peers.reconnect_base_delay_seconds) {
        throw std::invalid_argument("peers.reconnect_max_delay_seconds must be >= reconnect_base_delay_seconds");
    }
    if (peers.ban_threshold_errors < 1) {
        throw std::invalid_argument("peers.ban_threshold_errors must be >= 1");
    }
}
