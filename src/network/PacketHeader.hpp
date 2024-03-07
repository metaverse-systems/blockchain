#pragma once

#include <cstdint>

enum PacketType
{
    BLOCK,
    BLOCKCHAIN_QUERY,
    BLOCKCHAIN_RESPONSE
};

struct PacketHeader
{
    uint64_t length;
    uint64_t type;

    PacketHeader() : length(0), type(0) {}
    PacketHeader(uint64_t length, uint64_t type) : length(length), type(type) {}
};