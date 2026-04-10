#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/Blockchain.hpp"
#include "../src/MockChunk.hpp"
#include "../src/Chunk.hpp"
#include <filesystem>

TEST_CASE("Block Construction", "[Block]")
{
    // Arrange
    size_t index = 0;
    uint64_t time = 0;
    std::string prev_hash = "0";
    std::string data = "";

    // Act
    Block b(index, time, prev_hash, data);

    // Assert
    REQUIRE(b.index == index);
    REQUIRE(b.timestamp == time);
    REQUIRE(b.prevHash == prev_hash);
    REQUIRE(b.data == data);
    REQUIRE(!b.hash.empty()); // Hash should not be empty
}

TEST_CASE("Chunk::save() throws catchable std::runtime_error", "[Chunk]")
{
    // Attempt to save to a non-existent/read-only path
    Chunk c(0, "/nonexistent_dir_that_does_not_exist_xyz");
    REQUIRE_THROWS_AS(c.save(), std::runtime_error);
}

TEST_CASE("addBlock data integrity and chunk boundary", "[Blockchain]")
{
    Blockchain<MockChunk> bc(".");

    SECTION("Add 5 blocks and retrieve all by index")
    {
        std::vector<Block> added;
        for (int i = 0; i < 5; i++) {
            auto b = bc.addBlock("data_" + std::to_string(i), {"key_" + std::to_string(i)});
            added.push_back(b);
        }

        for (size_t i = 0; i < added.size(); i++) {
            Block retrieved = bc.getBlockByIndex(added[i].index);
            REQUIRE(retrieved.index == added[i].index);
            REQUIRE(retrieved.hash == added[i].hash);
            REQUIRE(retrieved.data == added[i].data);
        }
    }

    SECTION("Add 100 blocks to trigger new chunk boundary")
    {
        for (int i = 0; i < 100; i++) {
            bc.addBlock("block_" + std::to_string(i), {"key"});
        }

        // Genesis block is at index 0 in chunk 0
        // Blocks 1-99 fill chunk 0 (100 total with genesis)
        // Block 100 should trigger a new chunk
        Block last = bc.getBlockByIndex(100);
        REQUIRE(last.data == "block_99");

        // Verify earlier blocks still accessible
        Block first = bc.getBlockByIndex(1);
        REQUIRE(first.data == "block_0");
    }
}

TEST_CASE("getBlocksByKeys groups loads by chunk", "[Blockchain]")
{
    Blockchain<MockChunk> bc(".");

    // Add blocks with distinct keys that all map to the same chunk (chunk 0)
    bc.addBlock("a", {"alpha"});
    bc.addBlock("b", {"beta"});
    bc.addBlock("c", {"alpha"}); // same key, same chunk

    // Query with multiple keys — all blocks in chunk 0
    auto blocks = bc.getBlocksByKeys({"alpha", "beta"});

    // Should return 3 blocks (2 for alpha, 1 for beta)
    REQUIRE(blocks.size() == 3);
}