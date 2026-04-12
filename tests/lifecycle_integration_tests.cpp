#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include "TestHelpers.hpp"
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path create_test_dir(const std::string &name) {
    auto dir = std::filesystem::temp_directory_path() / ("lifecycle_integ_" + name);
    std::filesystem::create_directories(dir);
    return dir;
}

void cleanup_test_dir(const std::filesystem::path &dir) {
    std::filesystem::remove_all(dir);
}

ConsensusConfig test_config() {
    return TestHelpers::defaultConsensusConfig();
}

} // anonymous namespace

TEST_CASE("Multi-chunk shutdown/restart preserves all blocks", "[lifecycle][integration]") {
    auto dir = create_test_dir("shutdown_restart");

    size_t expectedBlockCount = 0;
    std::string lastBlockHash;

    // Phase 1: Create chain with 3+ chunks, save via saveAllChunks
    {
        Blockchain<Chunk> bc(dir, test_config());
        for (int i = 0; i < 250; i++) {
            bc.publish("test", "key" + std::to_string(i), "data" + std::to_string(i), {"k"});
        }
        expectedBlockCount = bc.getChainLength();
        REQUIRE(bc.getChunkCount() >= 3);

        // Simulate shutdown: freeze, then save
        bc.setShuttingDown();
        bc.saveAllChunks();
    }

    // Phase 2: Restart — clear in-memory state, run recoverChain
    {
        Blockchain<Chunk> bc2(dir, test_config());
        bc2.recoverChain();

        // Verify all blocks match
        REQUIRE(bc2.getChainLength() == expectedBlockCount);
        REQUIRE(bc2.getChunkCount() >= 3);

        // Verify blocks are accessible from any chunk
        Block first = bc2.getBlockByIndex(0);
        REQUIRE(first.index == 0);

        Block mid = bc2.getBlockByIndex(150);
        REQUIRE(mid.index == 150);

        Block last = bc2.getBlockByIndex(expectedBlockCount - 1);
        REQUIRE(last.index == expectedBlockCount - 1);
    }

    cleanup_test_dir(dir);
}

TEST_CASE("Block propagation rejects blocks after shutdown", "[lifecycle][integration]") {
    auto dir = create_test_dir("propagation_shutdown");
    Blockchain<Chunk> bc(dir, test_config());

    // Publish a valid block first so we have a chain to work with
    bc.publish("test", "key1", "data1", {"k"});

    // Set shutting down
    bc.setShuttingDown();

    // Verify appendBlock throws during shutdown (this is what BlockPropagation calls)
    Block candidate;
    candidate.index = bc.getChainLength();
    candidate.timestamp = static_cast<uint64_t>(std::time(nullptr));
    candidate.prevHash = bc.getBlockByIndex(bc.getChainLength() - 1).hash;
    candidate.difficulty = 0;
    candidate.nonce = 0;
    candidate.hash = candidate.calculateHash();

    REQUIRE_THROWS_AS(bc.appendBlock(candidate), std::runtime_error);
    REQUIRE_THROWS_WITH(bc.appendBlock(candidate), Catch::Matchers::ContainsSubstring("shutting down"));

    cleanup_test_dir(dir);
}
