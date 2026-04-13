#include "DifficultyEngine.hpp"
#include <cmath>

uint32_t DifficultyEngine::calculateNewDifficulty(const ConsensusConfig& config,
                                                    size_t totalBlockCount,
                                                    uint32_t currentDifficulty,
                                                    std::function<Block(size_t)> getBlock)
{
    if (totalBlockCount < config.adjustmentWindow + 1) {
        return currentDifficulty;
    }

    size_t windowEnd = totalBlockCount - 1;
    size_t windowStart = windowEnd - config.adjustmentWindow;

    Block firstBlock = getBlock(windowStart);
    Block lastBlock = getBlock(windowEnd);

    double expectedTime = static_cast<double>(config.targetBlockInterval) * config.adjustmentWindow;
    double actualTime = static_cast<double>(lastBlock.timestamp - firstBlock.timestamp);

    if (actualTime <= 0) actualTime = 1.0;

    double ratio = expectedTime / actualTime;

    double maxFactor = config.maxAdjustmentFactor;
    if (ratio > maxFactor) ratio = maxFactor;
    if (ratio < 1.0 / maxFactor) ratio = 1.0 / maxFactor;

    int32_t adjustment = static_cast<int32_t>(std::round(std::log2(ratio)));
    int32_t newDiff = static_cast<int32_t>(currentDifficulty) + adjustment;

    if (newDiff < static_cast<int32_t>(config.minDifficulty))
        newDiff = static_cast<int32_t>(config.minDifficulty);
    if (newDiff > static_cast<int32_t>(config.maxDifficulty))
        newDiff = static_cast<int32_t>(config.maxDifficulty);

    return static_cast<uint32_t>(newDiff);
}

uint32_t DifficultyEngine::getDifficultyForHeight(size_t height,
                                                    const ConsensusConfig& config,
                                                    size_t totalBlockCount,
                                                    std::unordered_map<size_t, uint32_t>& difficultyCache,
                                                    std::function<Block(size_t)> getBlock,
                                                    std::function<void(size_t)> retainChunk)
{
    if (height == 0) return 0;

    uint32_t difficulty = config.initialDifficulty;

    for (size_t boundaryHeight = config.adjustmentWindow;
         boundaryHeight <= height;
         boundaryHeight += config.adjustmentWindow)
    {
        auto cacheIt = difficultyCache.find(boundaryHeight);
        if (cacheIt != difficultyCache.end()) {
            difficulty = cacheIt->second;
            continue;
        }

        size_t windowStart = boundaryHeight - config.adjustmentWindow;
        size_t windowEnd = boundaryHeight;

        if (windowEnd >= totalBlockCount) break;

        retainChunk(windowStart);
        retainChunk(windowEnd);

        Block firstBlock = getBlock(windowStart);
        Block lastBlock = getBlock(windowEnd);

        double expectedTime = static_cast<double>(config.targetBlockInterval) * config.adjustmentWindow;
        double actualTime = static_cast<double>(lastBlock.timestamp - firstBlock.timestamp);
        if (actualTime <= 0) actualTime = 1.0;

        double ratio = expectedTime / actualTime;
        double maxFactor = config.maxAdjustmentFactor;
        if (ratio > maxFactor) ratio = maxFactor;
        if (ratio < 1.0 / maxFactor) ratio = 1.0 / maxFactor;

        int32_t adjustment = static_cast<int32_t>(std::round(std::log2(ratio)));
        int32_t newDiff = static_cast<int32_t>(difficulty) + adjustment;

        if (newDiff < static_cast<int32_t>(config.minDifficulty))
            newDiff = static_cast<int32_t>(config.minDifficulty);
        if (newDiff > static_cast<int32_t>(config.maxDifficulty))
            newDiff = static_cast<int32_t>(config.maxDifficulty);

        difficulty = static_cast<uint32_t>(newDiff);
        difficultyCache[boundaryHeight] = difficulty;
    }

    return difficulty;
}
