#pragma once
#include "utils.hpp"
#include "StreamEntry.hpp"
#include "MerkleTree.hpp"
#include <cstdint>
#include <vector>
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/string.hpp>
#include "json.hpp"

struct Block {
    size_t index;
    uint64_t timestamp;
    std::vector<StreamEntry> entries;
    std::string prevHash;
    std::string hash;
    uint64_t nonce;
    uint32_t difficulty;
    std::string merkleRoot;

    Block();
    Block(size_t index, uint64_t timestamp, std::string prevHash,
          std::vector<StreamEntry> entries, uint64_t nonce = 0, uint32_t difficulty = 0);
    std::string calculateHash() const;
    void dump();

    friend class boost::serialization::access;

    template<class Archive>
    unsigned int serialize(Archive& ar, const unsigned int version)
    {
        ar & index;
        ar & timestamp;
        ar & entries;
        ar & prevHash;
        ar & hash;
        ar & nonce;
        ar & difficulty;
        ar & merkleRoot;
        return version;
    }

    nlohmann::json toJson() const;
    nlohmann::json toHeaderJson() const;
};