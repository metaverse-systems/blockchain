#pragma once

#include "IChunk.hpp"

class MockChunk : public IChunk
{
  public:
    MockChunk(std::size_t index, std::filesystem::path blockchainPath) : IChunk(index, blockchainPath) {};
    void save() {};
    void load() {};
};