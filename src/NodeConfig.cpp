#include "NodeConfig.hpp"
#include <stdexcept>
#include <set>
#include <map>

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
            {"timeout_seconds", 30},
            {"log_level", "info"}
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
        }},
        {"streams", {
            {"allowed_streams", nlohmann::json::array()}
        }},
        {"persistence", {
            {"save_interval_seconds", 300},
            {"fast_startup", false}
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
        if (n.contains("log_level")) n["log_level"].get_to(cfg.network.log_level);
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

    // Streams
    if (j.contains("streams")) {
        auto &s = j["streams"];
        if (s.contains("allowed_streams") && s["allowed_streams"].is_array()) {
            cfg.streams.allowed_streams = s["allowed_streams"].get<std::vector<std::string>>();
        }
    }

    // Persistence
    if (j.contains("persistence")) {
        auto &p = j["persistence"];
        if (p.contains("save_interval_seconds")) p["save_interval_seconds"].get_to(cfg.persistence.save_interval_seconds);
        if (p.contains("fast_startup")) p["fast_startup"].get_to(cfg.persistence.fast_startup);
    }

    // Detect unknown keys
    static const std::set<std::string> known_top = {"tls", "network", "consensus", "peers", "streams", "persistence"};
    static const std::map<std::string, std::set<std::string>> known_sub = {
        {"tls", {"cert_file", "key_file", "ca_file"}},
        {"network", {"rpc_port", "p2p_port", "timeout_seconds", "log_level"}},
        {"consensus", {"target_block_interval", "adjustment_window", "max_adjustment_factor",
                        "min_difficulty", "max_difficulty", "initial_difficulty",
                        "mining_timeout", "max_future_timestamp", "max_reorg_depth"}},
        {"peers", {"seed_nodes", "max_outbound", "max_inbound", "exchange_interval_seconds",
                   "discovery_enabled", "max_stored_peers", "reconnect_base_delay_seconds",
                   "reconnect_max_delay_seconds", "ban_threshold_errors", "ban_duration_seconds"}},
        {"streams", {"allowed_streams"}},
        {"persistence", {"save_interval_seconds", "fast_startup"}}
    };
    for (auto &[key, val] : j.items()) {
        if (known_top.find(key) == known_top.end()) {
            logMessage("WARN", "Unknown config key: " + key);
        } else if (val.is_object()) {
            auto it = known_sub.find(key);
            if (it != known_sub.end()) {
                for (auto &[sub_key, sub_val] : val.items()) {
                    if (it->second.find(sub_key) == it->second.end()) {
                        logMessage("WARN", "Unknown config key: " + key + "." + sub_key);
                    }
                }
            }
        }
    }

    cfg.validate();
    return cfg;
}

void NodeConfig::validate(const std::filesystem::path &blockchain_dir) const {
    std::vector<std::string> errors;

    if (tls.cert_file.empty()) {
        errors.push_back("Error: tls.cert_file must be non-empty");
    }
    if (tls.key_file.empty()) {
        errors.push_back("Error: tls.key_file must be non-empty");
    }
    if (network.rpc_port == 0) {
        errors.push_back("Error: network.rpc_port: value " + std::to_string(network.rpc_port) + " out of valid range 1-65535");
    }
    if (network.p2p_port == 0) {
        errors.push_back("Error: network.p2p_port: value " + std::to_string(network.p2p_port) + " out of valid range 1-65535");
    }
    if (network.rpc_port == network.p2p_port) {
        errors.push_back("Error: network.rpc_port and network.p2p_port must not be equal (both are " + std::to_string(network.rpc_port) + ")");
    }
    if (!blockchain_dir.empty()) {
        auto resolve = [&](const std::string &p) -> std::filesystem::path {
            std::filesystem::path fp(p);
            return fp.is_absolute() ? fp : blockchain_dir / fp;
        };
        if (!tls.cert_file.empty() && !std::filesystem::exists(resolve(tls.cert_file))) {
            errors.push_back("Error: tls.cert_file: file not found: " + resolve(tls.cert_file).string());
        }
        if (!tls.key_file.empty() && !std::filesystem::exists(resolve(tls.key_file))) {
            errors.push_back("Error: tls.key_file: file not found: " + resolve(tls.key_file).string());
        }
    }
    if (peers.max_outbound == 0) {
        errors.push_back("Error: peers.max_outbound must be > 0");
    }
    if (peers.max_inbound == 0) {
        errors.push_back("Error: peers.max_inbound must be > 0");
    }
    if (peers.exchange_interval_seconds < 5) {
        errors.push_back("Error: peers.exchange_interval_seconds must be >= 5");
    }
    if (peers.reconnect_base_delay_seconds < 1) {
        errors.push_back("Error: peers.reconnect_base_delay_seconds must be >= 1");
    }
    if (peers.reconnect_max_delay_seconds < peers.reconnect_base_delay_seconds) {
        errors.push_back("Error: peers.reconnect_max_delay_seconds must be >= reconnect_base_delay_seconds");
    }
    if (peers.ban_threshold_errors < 1) {
        errors.push_back("Error: peers.ban_threshold_errors must be >= 1");
    }

    if (!errors.empty()) {
        std::string msg;
        for (const auto &e : errors) {
            msg += e + "\n";
        }
        throw std::invalid_argument(msg);
    }
}

void NodeConfig::generate_readme(const std::filesystem::path &readme_path) {
    std::ofstream ofs(readme_path);
    if (!ofs.is_open()) {
        throw std::runtime_error("Cannot create readme file: " + readme_path.string());
    }
    ofs << "# config.json Reference\n\n"
        << "## tls\n"
        << "  cert_file  (string)  TLS certificate file path        default: cert.pem\n"
        << "  key_file   (string)  TLS private key file path        default: key.pem\n"
        << "  ca_file    (string)  CA certificate for peer verify   default: (empty)\n\n"
        << "## network\n"
        << "  rpc_port          (uint16)  JSON-RPC listen port       default: 12345\n"
        << "  p2p_port          (uint16)  P2P listen port            default: 12346\n"
        << "  timeout_seconds   (uint32)  Network timeout            default: 30\n"
        << "  log_level         (string)  Log verbosity: debug|info|warning|error  default: info\n\n"
        << "## consensus\n"
        << "  target_block_interval  (uint32)  Seconds between blocks     default: 10\n"
        << "  adjustment_window      (uint32)  Blocks per difficulty adj   default: 10\n"
        << "  max_adjustment_factor  (double)  Max difficulty multiplier   default: 4.0\n"
        << "  min_difficulty         (uint32)  Minimum difficulty          default: 1\n"
        << "  max_difficulty         (uint32)  Maximum difficulty          default: 16\n"
        << "  initial_difficulty     (uint32)  Starting difficulty         default: 1\n"
        << "  mining_timeout         (uint32)  Mining timeout seconds      default: 30\n"
        << "  max_future_timestamp   (uint32)  Max future timestamp sec    default: 120\n"
        << "  max_reorg_depth        (uint32)  Max chain reorganization    default: 100\n\n"
        << "## peers\n"
        << "  seed_nodes                    (array)   Initial peer addresses      default: []\n"
        << "  max_outbound                  (uint32)  Max outbound connections    default: 8\n"
        << "  max_inbound                   (uint32)  Max inbound connections     default: 32\n"
        << "  exchange_interval_seconds     (uint32)  Peer exchange interval      default: 30\n"
        << "  discovery_enabled             (bool)    Enable peer discovery       default: true\n"
        << "  max_stored_peers              (uint32)  Max stored peer addresses   default: 256\n"
        << "  reconnect_base_delay_seconds  (uint32)  Reconnect base delay       default: 5\n"
        << "  reconnect_max_delay_seconds   (uint32)  Reconnect max delay        default: 300\n"
        << "  ban_threshold_errors          (uint32)  Errors before banning      default: 10\n"
        << "  ban_duration_seconds          (uint32)  Ban duration               default: 3600\n\n"
        << "## streams\n"
        << "  allowed_streams  (array)  Permitted stream names  default: []\n\n"
        << "## persistence\n"
        << "  save_interval_seconds  (uint32)  Auto-save interval              default: 300\n"
        << "  fast_startup           (bool)    Skip chunk validation on start   default: false\n";
}
