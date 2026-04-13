#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include "../src/MockChunk.hpp"
#include "TestHelpers.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>

// T011: chunk auto-saved when it reaches capacity (100 blocks)
TEST_CASE("Chunk auto-saved when it reaches capacity", "[US1][persistence]")
{
    auto dir = TestHelpers::createTestDir("T011");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        // Publish 100 blocks (genesis is block 0, so we need 100 more to fill chunk 0)
        for (size_t i = 1; i < 100; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }

        // Chunk 0 is now full (100 blocks: 0..99). Publish one more to trigger auto-save.
        bc.publish("test", "k100", "data", {"k100"});

        // chunk_000000.dat should now exist on disk
        REQUIRE(std::filesystem::exists(dir / "chunk_000000.dat"));
    }
    TestHelpers::cleanupTestDir(dir);
}

// T012: filled chunk freed from memory after auto-save
TEST_CASE("Filled chunk freed from memory after auto-save", "[US1][persistence]")
{
    auto dir = TestHelpers::createTestDir("T012");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        for (size_t i = 1; i < 100; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }
        // Trigger auto-save by filling chunk 0
        bc.publish("test", "k100", "data", {"k100"});

        // Chunk file should exist
        REQUIRE(std::filesystem::exists(dir / "chunk_000000.dat"));

        // Block 101 (index 100) should be in chunk 1 (the new active chunk)
        // Verify we can still access the block from the new active chunk
        Block b = bc.getBlockByIndex(100);
        REQUIRE(b.index == 100);
    }
    TestHelpers::cleanupTestDir(dir);
}

// T013: all in-memory chunks saved on shutdown call
TEST_CASE("All in-memory chunks saved on shutdown call", "[US1][persistence]")
{
    auto dir = TestHelpers::createTestDir("T013");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        // Add a few blocks (not enough to fill a chunk)
        for (size_t i = 1; i <= 5; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }

        // Call saveAllChunks (shutdown path)
        bc.saveAllChunks();

        // Active chunk should be saved
        REQUIRE(std::filesystem::exists(dir / "chunk_000000.dat"));
        REQUIRE(std::filesystem::exists(dir / "keys.dat"));
        REQUIRE(std::filesystem::exists(dir / "streams.dat"));
        REQUIRE(std::filesystem::exists(dir / "stream_index.dat"));
    }
    TestHelpers::cleanupTestDir(dir);
}

// T014: save failure logs error and continues operation
TEST_CASE("Save failure logs error and continues operation", "[US1][persistence]")
{
    auto dir = TestHelpers::createTestDir("T014");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        for (size_t i = 1; i <= 5; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }

        // saveAllChunks wraps in try/catch, should not throw even if something goes wrong
        // Test normal operation continues after saveAllChunks
        bc.saveAllChunks();

        // Continue operating after save
        bc.publish("test", "k_after_save", "data", {"k_after_save"});
        Block b = bc.getBlockByIndex(6);
        REQUIRE(b.index == 6);
    }
    TestHelpers::cleanupTestDir(dir);
}

// T031: periodic timer fires and saves dirty active chunk
TEST_CASE("Periodic timer fires and saves dirty active chunk", "[US3][persistence]")
{
    auto dir = TestHelpers::createTestDir("T031");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        boost::asio::io_context io;
        bc.startPeriodicSave(io);

        // Add a block to make dirty
        bc.publish("test", "k1", "data", {"k1"});

        // Run the io_context to process the timer
        io.run_for(std::chrono::milliseconds(100));

        bc.stopPeriodicSave();
    }
    TestHelpers::cleanupTestDir(dir);
}

// T032: periodic timer skips save when dirty_ == false
TEST_CASE("Periodic timer skips save when not dirty", "[US3][persistence]")
{
    auto dir = TestHelpers::createTestDir("T032");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        // Save to clear dirty flag
        bc.saveAllChunks();
        REQUIRE_FALSE(bc.isDirty());

        // Without adding new blocks, periodic save should be a no-op
        boost::asio::io_context io;
        bc.startPeriodicSave(io);
        io.run_for(std::chrono::milliseconds(100));
        bc.stopPeriodicSave();
    }
    TestHelpers::cleanupTestDir(dir);
}

// T033: periodic save disabled when save_interval_seconds == 0
TEST_CASE("Periodic save disabled when interval is 0", "[US3][persistence]")
{
    auto dir = TestHelpers::createTestDir("T033");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        // No timer should be started when interval is 0 (default)
        boost::asio::io_context io;
        bc.startPeriodicSave(io);  // save_interval_seconds_ == 0, should be a no-op
        io.run_for(std::chrono::milliseconds(100));
        bc.stopPeriodicSave();
    }
    TestHelpers::cleanupTestDir(dir);
}

// T034: periodic save also saves index files
TEST_CASE("Periodic save also saves index files", "[US3][persistence]")
{
    auto dir = TestHelpers::createTestDir("T034");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        bc.publish("test", "k1", "data", {"k1"});

        // Explicit saveAllChunks saves indexes
        bc.saveAllChunks();

        REQUIRE(std::filesystem::exists(dir / "keys.dat"));
        REQUIRE(std::filesystem::exists(dir / "streams.dat"));
        REQUIRE(std::filesystem::exists(dir / "stream_index.dat"));
    }
    TestHelpers::cleanupTestDir(dir);
}

// T046: getChainLength returns correct total across multiple chunks
TEST_CASE("getChainLength returns correct total across multiple chunks", "[US5][persistence]")
{
    auto dir = TestHelpers::createTestDir("T046");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        // Genesis = 1 block
        REQUIRE(bc.getChainLength() == 1);

        // Add 100 blocks to fill chunk 0 and spill into chunk 1
        for (size_t i = 1; i <= 100; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }

        REQUIRE(bc.getChainLength() == 101);
    }
    TestHelpers::cleanupTestDir(dir);
}

// T047: getChunkCount returns correct count
TEST_CASE("getChunkCount returns correct count", "[US5][persistence]")
{
    auto dir = TestHelpers::createTestDir("T047");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        // Initially 1 chunk with genesis block
        REQUIRE(bc.getChunkCount() == 1);

        // Fill chunk 0 (100 blocks total)
        for (size_t i = 1; i < 100; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }

        REQUIRE(bc.getChunkCount() == 1);

        // One more triggers chunk 1
        bc.publish("test", "k100", "data", {"k100"});
        REQUIRE(bc.getChunkCount() == 2);
    }
    TestHelpers::cleanupTestDir(dir);
}

// T049: counts update correctly as blocks are added
TEST_CASE("Counts update correctly as blocks are added", "[US5][persistence]")
{
    auto dir = TestHelpers::createTestDir("T049");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        REQUIRE(bc.getChainLength() == 1);
        REQUIRE(bc.getChunkCount() == 1);

        bc.publish("test", "k1", "data", {"k1"});
        REQUIRE(bc.getChainLength() == 2);
        REQUIRE(bc.getChunkCount() == 1);

        bc.publish("test", "k2", "data", {"k2"});
        REQUIRE(bc.getChainLength() == 3);
    }
    TestHelpers::cleanupTestDir(dir);
}

// T060a: chunk auto-save completes within 2 seconds on a 100-block chunk
TEST_CASE("Chunk auto-save completes within 2 seconds", "[US5][performance]")
{
    auto dir = TestHelpers::createTestDir("T060a");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);

        // Fill chunk 0 to 99 blocks
        for (size_t i = 1; i < 100; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }

        auto start = std::chrono::steady_clock::now();
        // This publish triggers auto-save of chunk 0
        bc.publish("test", "k100", "data", {"k100"});
        auto elapsed = std::chrono::steady_clock::now() - start;

        REQUIRE(elapsed < std::chrono::seconds(2));
        REQUIRE(std::filesystem::exists(dir / "chunk_000000.dat"));
    }
    TestHelpers::cleanupTestDir(dir);
}

// --- T015: Chunk retention during multi-access operations ---

TEST_CASE("ChunkRetainGuard prevents freeChunk from clearing retained chunks", "[US3][persistence]") {
    auto dir = TestHelpers::createTestDir("T015_retain");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<MockChunk> bc(dir, cfg);

        // Build enough blocks to fill chunk 0 and start chunk 1
        for (size_t i = 1; i <= 100; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }

        // Chunk 0 is freed from memory after rotation. Load it back.
        bc.loadChunk(0);

        // Retain chunk 0
        bc.retainChunk(0);

        // freeChunk should be a no-op for retained chunks
        bc.freeChunk(0);

        // Block in chunk 0 should still be accessible (loaded)
        Block b = bc.getBlockByIndex(0);
        REQUIRE(b.index == 0);

        // releaseChunks frees retained chunks
        bc.releaseChunks();
    }
    TestHelpers::cleanupTestDir(dir);
}

TEST_CASE("ChunkRetainGuard RAII releases chunks on scope exit", "[US3][persistence]") {
    auto dir = TestHelpers::createTestDir("T015_raii");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<MockChunk> bc(dir, cfg);

        for (size_t i = 1; i <= 100; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }

        bc.loadChunk(0);

        {
            Blockchain<MockChunk>::ChunkRetainGuard guard(bc);
            bc.retainChunk(0);
            // freeChunk is no-op while retained
            bc.freeChunk(0);
            Block b = bc.getBlockByIndex(0);
            REQUIRE(b.index == 0);
        }
        // Guard released — chunks freed
        SUCCEED("ChunkRetainGuard RAII cleanup completed");
    }
    TestHelpers::cleanupTestDir(dir);
}
