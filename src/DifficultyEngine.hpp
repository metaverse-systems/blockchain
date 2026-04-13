#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <unordered_map>
#include "Block.hpp"
#include "ConsensusConfig.hpp"

class DifficultyEngine
{
  public:
    DifficultyEngine() = default;

    uint32_t calculateNewDifficulty(const ConsensusConfig& config,
                                     size_t totalBlockCount,
                                     uint32_t currentDifficulty,
                                     std::function<Block(size_t)> getBlock);

    uint32_t getDifficultyForHeight(size_t height,
                                     const ConsensusConfig& config,
                                     size_t totalBlockCount,
                                     std::unordered_map<size_t, uint32_t>& difficultyCache,
                                     std::function<Block(size_t)> getBlock,
                                     std::function<void(size_t)> retainChunk);
};
