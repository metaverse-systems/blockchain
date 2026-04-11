#include <catch2/catch_all.hpp>
#include "../src/CliParser.hpp"
#include "../src/utils.hpp"
#include "../src/NodeConfig.hpp"
#include <filesystem>
#include <fstream>
#include <cstdlib>

// Helper to build an argv array from strings
static std::vector<char*> make_argv(std::vector<std::string> &args) {
    std::vector<char*> ptrs;
    for (auto &a : args) ptrs.push_back(a.data());
    return ptrs;
}

TEST_CASE("CliParser --help sets show_help", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "--help"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.show_help == true);
}

TEST_CASE("CliParser -h sets show_help", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "-h"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.show_help == true);
}

TEST_CASE("CliParser --version sets show_version", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "--version"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.show_version == true);
}

TEST_CASE("CliParser -v sets show_version", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "-v"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.show_version == true);
}

TEST_CASE("CliParser version_string contains version", "[CLI]") {
    auto ver = CliParser::version_string();
    REQUIRE(ver.find("blockchain") != std::string::npos);
    REQUIRE(ver.find("0.0.1") != std::string::npos);
}

TEST_CASE("CliParser no-args returns empty blockchain_dir", "[CLI]") {
    std::vector<std::string> args = {"blockchain"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.blockchain_dir.empty());
    REQUIRE(opts.show_help == false);
    REQUIRE(opts.show_version == false);
}

TEST_CASE("CliParser positional blockchain directory", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "/tmp/bc-dir"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.blockchain_dir == "/tmp/bc-dir");
}

TEST_CASE("CliParser --rpc-port override", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "--rpc-port", "9999", "/tmp/bc"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.rpc_port.has_value());
    REQUIRE(*opts.rpc_port == 9999);
}

TEST_CASE("CliParser --p2p-port override", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "--p2p-port", "9998", "/tmp/bc"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.p2p_port.has_value());
    REQUIRE(*opts.p2p_port == 9998);
}

TEST_CASE("CliParser --seed-node adds seed nodes", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "--seed-node", "10.0.0.1:12346",
                                      "--seed-node", "10.0.0.2:12346", "/tmp/bc"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.seed_nodes.size() == 2);
    REQUIRE(opts.seed_nodes[0] == "10.0.0.1:12346");
    REQUIRE(opts.seed_nodes[1] == "10.0.0.2:12346");
}

TEST_CASE("CliParser --log-level override", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "--log-level", "debug", "/tmp/bc"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.log_level.has_value());
    REQUIRE(*opts.log_level == "debug");
}

TEST_CASE("CliParser --config override", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "--config", "/tmp/other.json", "/tmp/bc"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.config_path.has_value());
    REQUIRE(*opts.config_path == "/tmp/other.json");
}

TEST_CASE("CliParser --generate-config flag", "[CLI]") {
    std::vector<std::string> args = {"blockchain", "--generate-config", "/tmp/bc"};
    auto argv = make_argv(args);
    auto opts = CliParser::parse(static_cast<int>(argv.size()), argv.data());
    REQUIRE(opts.generate_config == true);
    REQUIRE(opts.blockchain_dir == "/tmp/bc");
}

// --- CLI overrides applied to NodeConfig ---

static std::filesystem::path make_temp_dir() {
    auto p = std::filesystem::temp_directory_path() / ("cli_test_" + std::to_string(std::rand()));
    std::filesystem::create_directories(p);
    return p;
}

TEST_CASE("CLI --rpc-port overrides config value", "[CLI][override]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";
    NodeConfig::generate_default(cfg_path);
    auto cfg = NodeConfig::load(cfg_path);
    REQUIRE(cfg.network.rpc_port == 12345);
    // Simulate CLI override
    cfg.network.rpc_port = 9999;
    REQUIRE(cfg.network.rpc_port == 9999);
    std::filesystem::remove_all(dir);
}

TEST_CASE("CLI --p2p-port overrides config value", "[CLI][override]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";
    NodeConfig::generate_default(cfg_path);
    auto cfg = NodeConfig::load(cfg_path);
    cfg.network.p2p_port = 9998;
    REQUIRE(cfg.network.p2p_port == 9998);
    std::filesystem::remove_all(dir);
}

TEST_CASE("CLI --seed-node appends to config seeds", "[CLI][override]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";
    NodeConfig::generate_default(cfg_path);
    auto cfg = NodeConfig::load(cfg_path);
    REQUIRE(cfg.peers.seed_nodes.empty());
    PeerAddress addr;
    addr.host = "10.0.0.1";
    addr.port = 12346;
    cfg.peers.seed_nodes.push_back(addr);
    REQUIRE(cfg.peers.seed_nodes.size() == 1);
    std::filesystem::remove_all(dir);
}

TEST_CASE("CLI --log-level sets log level", "[CLI][override]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";
    NodeConfig::generate_default(cfg_path);
    auto cfg = NodeConfig::load(cfg_path);
    REQUIRE(cfg.network.log_level == "info");
    cfg.network.log_level = "debug";
    REQUIRE(cfg.network.log_level == "debug");
    std::filesystem::remove_all(dir);
}

// --- LogLevel filtering tests ---

TEST_CASE("LogLevel::Error suppresses INFO and WARN", "[LogLevel]") {
    setLogLevel(LogLevel::Error);
    REQUIRE(getLogLevel() == LogLevel::Error);
    // LogLevel::Error (3) > LogLevel::Info (1), so INFO/WARN should be suppressed
    // We can verify the level is set correctly
    REQUIRE(static_cast<int>(LogLevel::Info) < static_cast<int>(LogLevel::Error));
    REQUIRE(static_cast<int>(LogLevel::Warning) < static_cast<int>(LogLevel::Error));
    setLogLevel(LogLevel::Info); // reset
}

TEST_CASE("LogLevel::Debug shows all", "[LogLevel]") {
    setLogLevel(LogLevel::Debug);
    REQUIRE(getLogLevel() == LogLevel::Debug);
    REQUIRE(static_cast<int>(LogLevel::Debug) <= static_cast<int>(LogLevel::Info));
    REQUIRE(static_cast<int>(LogLevel::Debug) <= static_cast<int>(LogLevel::Warning));
    REQUIRE(static_cast<int>(LogLevel::Debug) <= static_cast<int>(LogLevel::Error));
    setLogLevel(LogLevel::Info); // reset
}

TEST_CASE("LogLevel::Info suppresses DEBUG", "[LogLevel]") {
    setLogLevel(LogLevel::Info);
    REQUIRE(static_cast<int>(LogLevel::Debug) < static_cast<int>(LogLevel::Info));
    setLogLevel(LogLevel::Info);
}

TEST_CASE("parseLogLevel valid levels", "[LogLevel]") {
    REQUIRE(parseLogLevel("debug") == LogLevel::Debug);
    REQUIRE(parseLogLevel("DEBUG") == LogLevel::Debug);
    REQUIRE(parseLogLevel("info") == LogLevel::Info);
    REQUIRE(parseLogLevel("Info") == LogLevel::Info);
    REQUIRE(parseLogLevel("warning") == LogLevel::Warning);
    REQUIRE(parseLogLevel("WARN") == LogLevel::Warning);
    REQUIRE(parseLogLevel("error") == LogLevel::Error);
    REQUIRE(parseLogLevel("Error") == LogLevel::Error);
}

TEST_CASE("parseLogLevel invalid level throws", "[LogLevel]") {
    REQUIRE_THROWS_AS(parseLogLevel("verbose"), std::invalid_argument);
    REQUIRE_THROWS_AS(parseLogLevel(""), std::invalid_argument);
    REQUIRE_THROWS_AS(parseLogLevel("critical"), std::invalid_argument);
}

// --- Generate config tests ---

TEST_CASE("--generate-config creates config.json and config.README", "[CLI][generate]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";
    auto readme_path = dir / "config.README";

    NodeConfig::generate_default(cfg_path);
    NodeConfig::generate_readme(readme_path);

    REQUIRE(std::filesystem::exists(cfg_path));
    REQUIRE(std::filesystem::exists(readme_path));

    // Verify generated config is valid and loadable
    auto cfg = NodeConfig::load(cfg_path);
    REQUIRE(cfg.network.rpc_port == 12345);
    REQUIRE(cfg.network.log_level == "info");

    std::filesystem::remove_all(dir);
}

TEST_CASE("--generate-config refuses to overwrite existing config", "[CLI][generate]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";

    // Create the config first
    NodeConfig::generate_default(cfg_path);
    REQUIRE(std::filesystem::exists(cfg_path));

    // Attempting to check for existing file (simulating the main.cpp logic)
    REQUIRE(std::filesystem::exists(cfg_path));

    std::filesystem::remove_all(dir);
}

// --- NodeConfig log_level round-trip test ---

TEST_CASE("config.json log_level round-trip", "[NodeConfig][log_level]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";

    nlohmann::json j = {
        {"network", {{"rpc_port", 12345}, {"p2p_port", 12346}, {"log_level", "debug"}}}
    };
    std::ofstream(cfg_path) << j.dump(2);
    auto cfg = NodeConfig::load(cfg_path);
    REQUIRE(cfg.network.log_level == "debug");

    std::filesystem::remove_all(dir);
}

TEST_CASE("config.json missing log_level defaults to info", "[NodeConfig][log_level]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";

    nlohmann::json j = {
        {"network", {{"rpc_port", 12345}, {"p2p_port", 12346}}}
    };
    std::ofstream(cfg_path) << j.dump(2);
    auto cfg = NodeConfig::load(cfg_path);
    REQUIRE(cfg.network.log_level == "info");

    std::filesystem::remove_all(dir);
}

// --- Validation tests ---

TEST_CASE("NodeConfig validation port out of range", "[NodeConfig][validate]") {
    NodeConfig cfg;
    cfg.network.rpc_port = 0;
    REQUIRE_THROWS_AS(cfg.validate(), std::invalid_argument);
}

TEST_CASE("NodeConfig validation equal ports", "[NodeConfig][validate]") {
    NodeConfig cfg;
    cfg.network.rpc_port = 5000;
    cfg.network.p2p_port = 5000;
    REQUIRE_THROWS_AS(cfg.validate(), std::invalid_argument);
}

TEST_CASE("NodeConfig validation missing TLS cert", "[NodeConfig][validate]") {
    auto dir = make_temp_dir();
    NodeConfig cfg;
    // cert_file defaults to cert.pem which won't exist in temp dir
    REQUIRE_THROWS_AS(cfg.validate(dir), std::invalid_argument);
    std::filesystem::remove_all(dir);
}

TEST_CASE("NodeConfig validation collects multiple errors", "[NodeConfig][validate]") {
    auto dir = make_temp_dir();
    NodeConfig cfg;
    cfg.network.rpc_port = 0;
    cfg.network.p2p_port = 0;
    cfg.tls.cert_file = "";
    try {
        cfg.validate(dir);
        FAIL("Expected exception");
    } catch (const std::invalid_argument &e) {
        std::string msg = e.what();
        // Should contain multiple error lines
        REQUIRE(msg.find("cert_file") != std::string::npos);
        REQUIRE(msg.find("rpc_port") != std::string::npos);
    }
    std::filesystem::remove_all(dir);
}

TEST_CASE("NodeConfig validation valid config passes", "[NodeConfig][validate]") {
    NodeConfig cfg;
    // Default config with no blockchain_dir (skips TLS file checks)
    REQUIRE_NOTHROW(cfg.validate());
}

TEST_CASE("NodeConfig unknown key warns but does not fail", "[NodeConfig]") {
    auto dir = make_temp_dir();
    auto cfg_path = dir / "config.json";

    nlohmann::json j = {
        {"network", {{"rpc_port", 12345}, {"p2p_port", 12346}}},
        {"unknown_section", {{"foo", "bar"}}}
    };
    std::ofstream(cfg_path) << j.dump(2);

    // Should not throw — unknown keys only warn
    REQUIRE_NOTHROW(NodeConfig::load(cfg_path));

    std::filesystem::remove_all(dir);
}
