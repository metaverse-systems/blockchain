#pragma once

#include <cstdint>
#include <vector>
#include "../Block.hpp"
#include <boost/serialization/serialization.hpp>
#include <boost/serialization/vector.hpp>

struct SyncQuery
{
    uint64_t local_chain_height = 0;

    friend class boost::serialization::access;

    template<class Archive>
    unsigned int serialize(Archive &ar, const unsigned int version)
    {
        ar & local_chain_height;
        return version;
    }
};

struct SyncResponse
{
    uint64_t total_chain_height = 0;
    uint64_t start_index = 0;
    std::vector<Block> blocks;

    friend class boost::serialization::access;

    template<class Archive>
    unsigned int serialize(Archive &ar, const unsigned int version)
    {
        ar & total_chain_height;
        ar & start_index;
        ar & blocks;
        return version;
    }
};
