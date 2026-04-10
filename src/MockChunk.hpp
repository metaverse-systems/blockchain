#pragma once

#include "IChunk.hpp"

class MockChunk : public IChunk
{
  public:
    int load_count = 0;
    MockChunk(std::size_t index, std::filesystem::path blockchainPath) : IChunk(index, blockchainPath) {};
    void save() {};
    void load() { load_count++; };
};