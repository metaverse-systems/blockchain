#include <catch2/catch_all.hpp>
#include "../src/DifficultyEngine.hpp"
#include "../src/Block.hpp"
#include "../src/ConsensusConfig.hpp"
#include <unordered_map>
#include <vector>
#include <functional>

namespace {

Block make_block_with_ts(size_t index, uint64_t timestamp) {
    Block b;
    b.index = index;
    b.timestamp = timestamp;
    b.difficulty = 0;
    b.nonce = 0;
    b.prevHash = "";
    b.hash = b.calculateHash();
    return b;
}

} // anonymous namespace

TEST_CASE("DifficultyEngine returns current difficulty below adjustment window", "[DifficultyEngine]")
{
    DifficultyEngine engine;
    ConsensusConfig config;
    config.adjustmentWindow = 10;
    config.initialDifficulty = 4;

    // Only 5 blocks, less than window + 1
    auto getBlock = [](size_t) -> Block { return Block(); };
    uint32_t result = engine.calculateNewDifficulty(config, 5, 4, getBlock);
    REQUIRE(result == 4);
}

TEST_CASE("DifficultyEngine calculateNewDifficulty at adjustment window", "[DifficultyEngine]")
{
    DifficultyEngine engine;
    ConsensusConfig config;
    config.adjustmentWindow = 10;
    config.targetBlockInterval = 10;
    config.maxAdjustmentFactor = 4.0;
    config.minDifficulty = 1;
    config.maxDifficulty = 16;
    config.initialDifficulty = 4;

    // Create blocks: perfectly timed (10s apart), so difficulty should not change
    std::vector<Block> blocks;
    for (size_t i = 0; i <= 10; i++) {
        blocks.push_back(make_block_with_ts(i, 1000 + i * 10));
    }

    auto getBlock = [&blocks](size_t idx) -> Block { return blocks[idx]; };
    uint32_t result = engine.calculateNewDifficulty(config, 11, 4, getBlock);
    REQUIRE(result == 4); // No change when timing is perfect
}

TEST_CASE("DifficultyEngine calculateNewDifficulty blocks too fast", "[DifficultyEngine]")
{
    DifficultyEngine engine;
    ConsensusConfig config;
    config.adjustmentWindow = 10;
    config.targetBlockInterval = 10;
    config.maxAdjustmentFactor = 4.0;
    config.minDifficulty = 1;
    config.maxDifficulty = 16;
    config.initialDifficulty = 4;

    // Blocks produced in 1s each (too fast -> difficulty should increase)
    std::vector<Block> blocks;
    for (size_t i = 0; i <= 10; i++) {
        blocks.push_back(make_block_with_ts(i, 1000 + i * 1));
    }

    auto getBlock = [&blocks](size_t idx) -> Block { return blocks[idx]; };
    uint32_t result = engine.calculateNewDifficulty(config, 11, 4, getBlock);
    REQUIRE(result > 4); // Difficulty should increase
    REQUIRE(result <= config.maxDifficulty);
}

TEST_CASE("DifficultyEngine calculateNewDifficulty blocks too slow", "[DifficultyEngine]")
{
    DifficultyEngine engine;
    ConsensusConfig config;
    config.adjustmentWindow = 10;
    config.targetBlockInterval = 10;
    config.maxAdjustmentFactor = 4.0;
    config.minDifficulty = 1;
    config.maxDifficulty = 16;
    config.initialDifficulty = 4;

    // Blocks produced in 100s each (too slow -> difficulty should decrease)
    std::vector<Block> blocks;
    for (size_t i = 0; i <= 10; i++) {
        blocks.push_back(make_block_with_ts(i, 1000 + i * 100));
    }

    auto getBlock = [&blocks](size_t idx) -> Block { return blocks[idx]; };
    uint32_t result = engine.calculateNewDifficulty(config, 11, 4, getBlock);
    REQUIRE(result < 4); // Difficulty should decrease
    REQUIRE(result >= config.minDifficulty);
}

TEST_CASE("DifficultyEngine clamps to min/max difficulty", "[DifficultyEngine]")
{
    DifficultyEngine engine;
    ConsensusConfig config;
    config.adjustmentWindow = 10;
    config.targetBlockInterval = 10;
    config.maxAdjustmentFactor = 4.0;
    config.minDifficulty = 3;
    config.maxDifficulty = 16;
    config.initialDifficulty = 3;

    // Extremely slow blocks with low starting difficulty
    std::vector<Block> blocks;
    for (size_t i = 0; i <= 10; i++) {
        blocks.push_back(make_block_with_ts(i, 1000 + i * 1000));
    }

    auto getBlock = [&blocks](size_t idx) -> Block { return blocks[idx]; };
    uint32_t result = engine.calculateNewDifficulty(config, 11, 3, getBlock);
    REQUIRE(result >= config.minDifficulty);
}

TEST_CASE("DifficultyEngine getDifficultyForHeight returns 0 for genesis", "[DifficultyEngine]")
{
    DifficultyEngine engine;
    ConsensusConfig config;
    std::unordered_map<size_t, uint32_t> cache;
    auto getBlock = [](size_t) -> Block { return Block(); };
    auto retainChunk = [](size_t) {};

    uint32_t result = engine.getDifficultyForHeight(0, config, 100, cache, getBlock, retainChunk);
    REQUIRE(result == 0);
}

TEST_CASE("DifficultyEngine getDifficultyForHeight cache hit", "[DifficultyEngine]")
{
    DifficultyEngine engine;
    ConsensusConfig config;
    config.adjustmentWindow = 10;
    config.initialDifficulty = 4;

    std::unordered_map<size_t, uint32_t> cache;
    cache[10] = 7;

    auto getBlock = [](size_t) -> Block { return Block(); };
    auto retainChunk = [](size_t) {};

    uint32_t result = engine.getDifficultyForHeight(10, config, 100, cache, getBlock, retainChunk);
    REQUIRE(result == 7);
}

TEST_CASE("DifficultyEngine getDifficultyForHeight cache miss computes", "[DifficultyEngine]")
{
    DifficultyEngine engine;
    ConsensusConfig config;
    config.adjustmentWindow = 10;
    config.targetBlockInterval = 10;
    config.maxAdjustmentFactor = 4.0;
    config.minDifficulty = 1;
    config.maxDifficulty = 16;
    config.initialDifficulty = 4;

    // Perfect timing
    std::vector<Block> blocks;
    for (size_t i = 0; i <= 20; i++) {
        blocks.push_back(make_block_with_ts(i, 1000 + i * 10));
    }

    std::unordered_map<size_t, uint32_t> cache;
    auto getBlock = [&blocks](size_t idx) -> Block { return blocks[idx]; };
    auto retainChunk = [](size_t) {};

    uint32_t result = engine.getDifficultyForHeight(15, config, 21, cache, getBlock, retainChunk);
    REQUIRE(result == config.initialDifficulty); // No change for perfect timing
    REQUIRE(cache.count(10) == 1); // Should have cached the boundary
}
