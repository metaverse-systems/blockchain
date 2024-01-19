#pragma once

#include <cstdint>

enum packet_type
{
    BLOCK,
    BLOCKCHAIN_QUERY,
    BLOCKCHAIN_RESPONSE
};

struct packet_header
{
    uint64_t length;
    uint64_t type;

    packet_header() : length(0), type(0) {}
    packet_header(uint64_t length, uint64_t type) : length(length), type(type) {}
};