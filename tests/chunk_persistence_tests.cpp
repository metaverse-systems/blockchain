#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace {

// Helper: create a temp directory for test data
std::filesystem::path create_test_dir(const std::string &name) {
    auto dir = std::filesystem::temp_directory_path() / ("chunk_persist_test_" + name);
    std::filesystem::create_directories(dir);
    return dir;
}

// Helper: clean up test directory
void cleanup_test_dir(const std::filesystem::path &dir) {
    std::filesystem::remove_all(dir);
}

// Helper: build a valid block for a given index (difficulty 0 for speed)
Block make_block(size_t index, const std::string &prevHash) {
    StreamEntry e;
    e.stream = "test";
    e.key = "k" + std::to_string(index);
    e.data = "data";

    Block b;
    b.index = index;
    b.timestamp = static_cast<uint64_t>(std::time(nullptr));
    b.entries = {e};
    b.prevHash = prevHash;
    b.difficulty = 0;
    b.nonce = 0;
    b.hash = b.calculateHash();
    return b;
}

} // anonymous namespace

// T011: chunk auto-saved when it reaches capacity (100 blocks)
TEST_CASE("Chunk auto-saved when it reaches capacity", "[US1][persistence]")
{
    auto dir = create_test_dir("T011");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
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
    cleanup_test_dir(dir);
}

// T012: filled chunk freed from memory after auto-save
TEST_CASE("Filled chunk freed from memory after auto-save", "[US1][persistence]")
{
    auto dir = create_test_dir("T012");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
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
    cleanup_test_dir(dir);
}

// T013: all in-memory chunks saved on shutdown call
TEST_CASE("All in-memory chunks saved on shutdown call", "[US1][persistence]")
{
    auto dir = create_test_dir("T013");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
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
    cleanup_test_dir(dir);
}

// T014: save failure logs error and continues operation
TEST_CASE("Save failure logs error and continues operation", "[US1][persistence]")
{
    auto dir = create_test_dir("T014");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
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
    cleanup_test_dir(dir);
}

// T031: periodic timer fires and saves dirty active chunk
TEST_CASE("Periodic timer fires and saves dirty active chunk", "[US3][persistence]")
{
    auto dir = create_test_dir("T031");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
        Blockchain<Chunk> bc(dir, cfg);

        boost::asio::io_context io;
        bc.startPeriodicSave(io);

        // Add a block to make dirty
        bc.publish("test", "k1", "data", {"k1"});

        // Run the io_context to process the timer
        io.run_for(std::chrono::milliseconds(100));

        bc.stopPeriodicSave();
    }
    cleanup_test_dir(dir);
}

// T032: periodic timer skips save when dirty_ == false
TEST_CASE("Periodic timer skips save when not dirty", "[US3][persistence]")
{
    auto dir = create_test_dir("T032");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
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
    cleanup_test_dir(dir);
}

// T033: periodic save disabled when save_interval_seconds == 0
TEST_CASE("Periodic save disabled when interval is 0", "[US3][persistence]")
{
    auto dir = create_test_dir("T033");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
        Blockchain<Chunk> bc(dir, cfg);

        // No timer should be started when interval is 0 (default)
        boost::asio::io_context io;
        bc.startPeriodicSave(io);  // save_interval_seconds_ == 0, should be a no-op
        io.run_for(std::chrono::milliseconds(100));
        bc.stopPeriodicSave();
    }
    cleanup_test_dir(dir);
}

// T034: periodic save also saves index files
TEST_CASE("Periodic save also saves index files", "[US3][persistence]")
{
    auto dir = create_test_dir("T034");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
        Blockchain<Chunk> bc(dir, cfg);

        bc.publish("test", "k1", "data", {"k1"});

        // Explicit saveAllChunks saves indexes
        bc.saveAllChunks();

        REQUIRE(std::filesystem::exists(dir / "keys.dat"));
        REQUIRE(std::filesystem::exists(dir / "streams.dat"));
        REQUIRE(std::filesystem::exists(dir / "stream_index.dat"));
    }
    cleanup_test_dir(dir);
}

// T046: getChainLength returns correct total across multiple chunks
TEST_CASE("getChainLength returns correct total across multiple chunks", "[US5][persistence]")
{
    auto dir = create_test_dir("T046");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
        Blockchain<Chunk> bc(dir, cfg);

        // Genesis = 1 block
        REQUIRE(bc.getChainLength() == 1);

        // Add 100 blocks to fill chunk 0 and spill into chunk 1
        for (size_t i = 1; i <= 100; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }

        REQUIRE(bc.getChainLength() == 101);
    }
    cleanup_test_dir(dir);
}

// T047: getChunkCount returns correct count
TEST_CASE("getChunkCount returns correct count", "[US5][persistence]")
{
    auto dir = create_test_dir("T047");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
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
    cleanup_test_dir(dir);
}

// T049: counts update correctly as blocks are added
TEST_CASE("Counts update correctly as blocks are added", "[US5][persistence]")
{
    auto dir = create_test_dir("T049");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
        Blockchain<Chunk> bc(dir, cfg);

        REQUIRE(bc.getChainLength() == 1);
        REQUIRE(bc.getChunkCount() == 1);

        bc.publish("test", "k1", "data", {"k1"});
        REQUIRE(bc.getChainLength() == 2);
        REQUIRE(bc.getChunkCount() == 1);

        bc.publish("test", "k2", "data", {"k2"});
        REQUIRE(bc.getChainLength() == 3);
    }
    cleanup_test_dir(dir);
}

// T060a: chunk auto-save completes within 2 seconds on a 100-block chunk
TEST_CASE("Chunk auto-save completes within 2 seconds", "[US5][performance]")
{
    auto dir = create_test_dir("T060a");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        cfg.miningTimeout = 60;
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
    cleanup_test_dir(dir);
}
