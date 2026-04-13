#pragma once

#include "PacketHeader.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <cstring>

// Shared packet serialization utility.
// Returns (header_bytes, serialized_payload) for callers to write via their own async_write.
template<typename T>
std::pair<std::vector<char>, std::string> serialize_packet(const T &obj, uint64_t packet_type)
{
    std::stringstream ss;
    {
        boost::archive::binary_oarchive oa(ss);
        oa << obj;
    }
    std::string serialized = ss.str();

    PacketHeader header(serialized.size(), packet_type);
    std::vector<char> header_data(sizeof(header));
    std::memcpy(header_data.data(), &header, sizeof(header));

    return {std::move(header_data), std::move(serialized)};
}
