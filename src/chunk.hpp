#pragma once
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "block.hpp"
#include "IChunk.hpp"

class chunk : public IChunk
{
  public:
    chunk(std::size_t index, std::filesystem::path blockchainPath) : IChunk(index, blockchainPath) {};
    void save();
    void load();
};