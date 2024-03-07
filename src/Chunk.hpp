#pragma once
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "Block.hpp"
#include "IChunk.hpp"

class Chunk : public IChunk
{
  public:
    Chunk(std::size_t index, std::filesystem::path blockchainPath) : IChunk(index, blockchainPath) {};
    void save();
    void load();
};