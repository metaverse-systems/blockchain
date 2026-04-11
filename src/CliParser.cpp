#include "CliParser.hpp"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <boost/program_options.hpp>
#include <sstream>

namespace po = boost::program_options;

static po::options_description make_options() {
    po::options_description desc("Options");
    desc.add_options()
        ("help,h", "Show this help message")
        ("version,v", "Show version information")
        ("config", po::value<std::string>(), "Path to configuration file")
        ("rpc-port", po::value<uint16_t>(), "RPC listen port (default: 12345)")
        ("p2p-port", po::value<uint16_t>(), "P2P listen port (default: 12346)")
        ("seed-node", po::value<std::vector<std::string>>(), "Add seed node (repeatable)")
        ("log-level", po::value<std::string>(), "Log level: debug|info|warning|error (default: info)")
        ("generate-config", "Generate default config.json and exit")
    ;
    return desc;
}

std::string CliParser::usage_string() {
    auto desc = make_options();
    std::ostringstream oss;
    oss << "Usage: blockchain [OPTIONS] <blockchain-directory>\n\n" << desc;
    return oss.str();
}

std::string CliParser::version_string() {
#ifdef PACKAGE_VERSION
    return std::string(PACKAGE_NAME) + " " + PACKAGE_VERSION;
#else
    return "blockchain unknown";
#endif
}

CliOptions CliParser::parse(int argc, char* argv[]) {
    auto desc = make_options();

    po::options_description hidden;
    hidden.add_options()
        ("blockchain-dir", po::value<std::string>(), "Path to blockchain data directory")
    ;

    po::options_description all;
    all.add(desc).add(hidden);

    po::positional_options_description pos;
    pos.add("blockchain-dir", 1);

    CliOptions opts;

    po::variables_map vm;
    po::store(po::command_line_parser(argc, argv)
        .options(all)
        .positional(pos)
        .run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
        opts.show_help = true;
        return opts;
    }
    if (vm.count("version")) {
        opts.show_version = true;
        return opts;
    }
    if (vm.count("generate-config")) {
        opts.generate_config = true;
    }

    if (vm.count("blockchain-dir")) {
        opts.blockchain_dir = vm["blockchain-dir"].as<std::string>();
    }
    if (vm.count("config")) {
        opts.config_path = vm["config"].as<std::string>();
    }
    if (vm.count("rpc-port")) {
        opts.rpc_port = vm["rpc-port"].as<uint16_t>();
    }
    if (vm.count("p2p-port")) {
        opts.p2p_port = vm["p2p-port"].as<uint16_t>();
    }
    if (vm.count("seed-node")) {
        opts.seed_nodes = vm["seed-node"].as<std::vector<std::string>>();
    }
    if (vm.count("log-level")) {
        opts.log_level = vm["log-level"].as<std::string>();
    }

    return opts;
}
