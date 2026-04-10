#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "json.hpp"
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

// Network endpoint for a peer
struct PeerAddress {
    std::string host;
    uint16_t port = 0;

    bool operator==(const PeerAddress &other) const {
        return host == other.host && port == other.port;
    }
    bool operator!=(const PeerAddress &other) const {
        return !(*this == other);
    }

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /*version*/) {
        ar & host;
        ar & port;
    }
};

inline void to_json(nlohmann::json &j, const PeerAddress &a) {
    j = nlohmann::json{{"host", a.host}, {"port", a.port}};
}

inline void from_json(const nlohmann::json &j, PeerAddress &a) {
    j.at("host").get_to(a.host);
    j.at("port").get_to(a.port);
}

// A known peer with associated runtime state (stored in peers.json)
struct PeerEntry {
    std::string host;
    uint16_t port = 0;
    std::string node_uuid;
    uint64_t last_seen = 0;
    uint32_t error_count = 0;

    bool operator==(const PeerEntry &other) const {
        return host == other.host && port == other.port;
    }
};

inline void to_json(nlohmann::json &j, const PeerEntry &e) {
    j = nlohmann::json{
        {"host", e.host},
        {"port", e.port},
        {"node_uuid", e.node_uuid},
        {"last_seen", e.last_seen},
        {"error_count", e.error_count}
    };
}

inline void from_json(const nlohmann::json &j, PeerEntry &e) {
    j.at("host").get_to(e.host);
    j.at("port").get_to(e.port);
    if (j.contains("node_uuid")) j.at("node_uuid").get_to(e.node_uuid);
    if (j.contains("last_seen")) j.at("last_seen").get_to(e.last_seen);
    if (j.contains("error_count")) j.at("error_count").get_to(e.error_count);
}

// Ban record for a misbehaving peer
struct BanRecord {
    std::string host;
    uint16_t port = 0;
    std::string reason;
    uint64_t expires = 0; // 0 = permanent until unban
};

inline void to_json(nlohmann::json &j, const BanRecord &b) {
    j = nlohmann::json{
        {"host", b.host},
        {"port", b.port},
        {"reason", b.reason},
        {"expires", b.expires}
    };
}

inline void from_json(const nlohmann::json &j, BanRecord &b) {
    j.at("host").get_to(b.host);
    j.at("port").get_to(b.port);
    if (j.contains("reason")) j.at("reason").get_to(b.reason);
    if (j.contains("expires")) j.at("expires").get_to(b.expires);
}

// Peer discovery configuration subset (passed to PeerManager)
struct PeerConfig {
    std::vector<PeerAddress> seed_nodes;
    uint16_t max_outbound = 8;
    uint16_t max_inbound = 32;
    uint32_t exchange_interval_seconds = 30;
    bool discovery_enabled = true;
    uint16_t max_stored_peers = 256;
    uint32_t reconnect_base_delay_seconds = 5;
    uint32_t reconnect_max_delay_seconds = 300;
    uint32_t ban_threshold_errors = 10;
    uint32_t ban_duration_seconds = 3600;
};
