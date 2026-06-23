#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

struct CliOptions {
    std::string blockchain_dir;
    std::optional<std::string> config_path;
    std::optional<uint16_t> rpc_port;
    std::optional<uint16_t> p2p_port;
    std::optional<uint16_t> monitoring_port;
    std::vector<std::string> seed_nodes;
    std::optional<std::string> log_level;
    bool generate_config = false;
    bool show_help = false;
    bool show_version = false;
};

class CliParser {
public:
    // Parse command-line arguments. Returns CliOptions.
    // Throws std::runtime_error on invalid arguments.
    static CliOptions parse(int argc, char* argv[]);

    // Return the formatted help/usage string.
    static std::string usage_string();

    // Return the version string (e.g., "blockchain 0.0.1").
    static std::string version_string();
};
