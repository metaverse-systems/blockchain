#pragma once
#include "utils.hpp"
#include <cstdint>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>


struct block {
    size_t index;
    uint64_t timestamp;
    std::string data;
    std::string prevHash;
    std::string hash;

    block();
    block(size_t, uint64_t, std::string, std::string);
    std::string calculateHash() const;
    void dump();

    friend class boost::serialization::access;

    template<class Archive>
    unsigned int serialize(Archive& ar, const unsigned int version)
    {
        ar & index;
        ar & timestamp;
        ar & data;
        ar & prevHash;
        ar & hash;
        return version;
    }
};