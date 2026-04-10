#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "../PeerConfig.hpp"
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/vector.hpp>

struct PeerExchangeRequest {
    std::string sender_uuid;
    uint16_t sender_listen_port = 0;
    std::vector<PeerAddress> peers;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /*version*/) {
        ar & sender_uuid;
        ar & sender_listen_port;
        ar & peers;
    }
};

struct PeerExchangeResponse {
    std::string sender_uuid;
    uint16_t sender_listen_port = 0;
    std::vector<PeerAddress> peers;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /*version*/) {
        ar & sender_uuid;
        ar & sender_listen_port;
        ar & peers;
    }
};
