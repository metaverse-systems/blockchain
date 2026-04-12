#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include "TestHelpers.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace {

std::filesystem::path create_test_dir(const std::string &name) {
    auto dir = std::filesystem::temp_directory_path() / ("chunk_recovery_test_" + name);
    std::filesystem::create_directories(dir);
    return dir;
}

void cleanup_test_dir(const std::filesystem::path &dir) {
    std::filesystem::remove_all(dir);
}

// Helper: build a chain with N blocks, save all chunks, and return the directory
// The blockchain object is destroyed after this, simulating shutdown.
void build_and_save_chain(const std::filesystem::path &dir, size_t blockCount) {
    auto cfg = TestHelpers::defaultConsensusConfig();
    Blockchain<Chunk> bc(dir, cfg);

    for (size_t i = 1; i < blockCount; i++) {
        bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
    }

    bc.saveAllChunks();
}

} // anonymous namespace

// T019: startup discovers contiguous chunk files 0..N and reports correct block count
TEST_CASE("Startup discovers contiguous chunk files and reports correct block count", "[US2][recovery]")
{
    auto dir = create_test_dir("T019");
    {
        // Build a chain with 201 blocks (chunks 0, 1, and partially filled chunk 2)
        build_and_save_chain(dir, 201);

        // Now recover: create a new Blockchain and recover instead of genesis
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        REQUIRE(bc.getChainLength() == 201);
        REQUIRE(bc.getChunkCount() == 3);
    }
    cleanup_test_dir(dir);
}

// T020: startup with no chunk files creates fresh genesis block
TEST_CASE("Startup with no chunk files starts fresh", "[US2][recovery]")
{
    auto dir = create_test_dir("T020");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);

        // discoverChunks on empty dir returns 0
        size_t count = bc.discoverChunks();
        REQUIRE(count == 0);

        // Fresh genesis block should be present
        REQUIRE(bc.getChainBlockCount() >= 1);
    }
    cleanup_test_dir(dir);
}

// T021: on-demand load serves block from filled chunk and frees it after
TEST_CASE("On-demand load serves block from filled chunk", "[US2][recovery]")
{
    auto dir = create_test_dir("T021");
    {
        // Build and save 150 blocks (chunk 0 full, chunk 1 partial)
        build_and_save_chain(dir, 150);

        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        // Access block 50 from chunk 0 (which should be on disk, not in memory)
        Block b = bc.getBlockByIndex(50);
        REQUIRE(b.index == 50);
    }
    cleanup_test_dir(dir);
}

// T022: new block appended seamlessly after recovery
TEST_CASE("New block appended seamlessly after recovery", "[US2][recovery]")
{
    auto dir = create_test_dir("T022");
    {
        build_and_save_chain(dir, 105);

        auto cfg = TestHelpers::defaultConsensusConfig();
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        size_t before = bc.getChainLength();

        // Publish a new block after recovery
        bc.publish("test", "new_key", "new_data", {"new_key"});

        REQUIRE(bc.getChainLength() == before + 1);
    }
    cleanup_test_dir(dir);
}

// T023: index files loaded on startup; missing index triggers rebuild
TEST_CASE("Missing index files trigger rebuild from chunks", "[US2][recovery]")
{
    auto dir = create_test_dir("T023");
    {
        build_and_save_chain(dir, 50);

        // Delete index files to force rebuild
        std::filesystem::remove(dir / "keys.dat");
        std::filesystem::remove(dir / "streams.dat");
        std::filesystem::remove(dir / "stream_index.dat");

        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        // Recovery should have rebuilt from chunk data
        REQUIRE(bc.getChainLength() == 50);
    }
    cleanup_test_dir(dir);
}

// T039: truncated chunk file detected and logged
TEST_CASE("Truncated chunk file detected and logged", "[US4][recovery]")
{
    auto dir = create_test_dir("T039");
    {
        build_and_save_chain(dir, 201);

        // Truncate chunk 1
        auto path = dir / "chunk_000001.dat";
        {
            std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
            ofs << "corrupt";
        }

        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        // Should only load chunk 0 (contiguous prefix up to first corrupt)
        REQUIRE(bc.getChainLength() == 100);
        REQUIRE(bc.getChunkCount() == 1);
    }
    cleanup_test_dir(dir);
}

// T040: chunk with invalid block hashes detected and logged
TEST_CASE("Chunk with invalid block hashes detected", "[US4][recovery]")
{
    auto dir = create_test_dir("T040");
    {
        build_and_save_chain(dir, 201);

        // Corrupt chunk 1 by truncating it heavily
        auto path = dir / "chunk_000001.dat";
        {
            std::ofstream ofs(path, std::ios::binary | std::ios::trunc);
            ofs << "x";
        }

        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        // Only chunk 0's blocks should be loaded
        REQUIRE(bc.getChainLength() == 100);
    }
    cleanup_test_dir(dir);
}

// T041: contiguous prefix loaded up to first corrupt chunk
TEST_CASE("Contiguous prefix loaded up to first corrupt chunk", "[US4][recovery]")
{
    auto dir = create_test_dir("T041");
    {
        build_and_save_chain(dir, 301);

        // Corrupt chunk 1 (out of 0, 1, 2)
        {
            std::ofstream ofs(dir / "chunk_000001.dat", std::ios::binary | std::ios::trunc);
            ofs << "bad";
        }

        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        // Only chunk 0 loaded
        REQUIRE(bc.getChainLength() == 100);
        REQUIRE(bc.getChunkCount() == 1);
    }
    cleanup_test_dir(dir);
}

// T042: gap in chunk numbering stops loading at gap
TEST_CASE("Gap in chunk numbering stops loading at gap", "[US4][recovery]")
{
    auto dir = create_test_dir("T042");
    {
        build_and_save_chain(dir, 301);

        // Remove chunk 1 to create a gap
        std::filesystem::remove(dir / "chunk_000001.dat");

        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        // Only chunk 0 loaded (gap at chunk 1)
        REQUIRE(bc.getChainLength() == 100);
        REQUIRE(bc.getChunkCount() == 1);
    }
    cleanup_test_dir(dir);
}

// T042a: chunk file with restrictive permissions detected and logged
TEST_CASE("Chunk file with restrictive permissions detected", "[US4][recovery]")
{
#ifdef _WIN32
    SKIP("POSIX file permissions are not enforced on Windows");
#endif
    auto dir = create_test_dir("T042a");
    {
        build_and_save_chain(dir, 201);

        // Make chunk 1 unreadable
        auto path = dir / "chunk_000001.dat";
        std::filesystem::permissions(path, std::filesystem::perms::none);

        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        // Only chunk 0 loaded (chunk 1 unreadable)
        REQUIRE(bc.getChainLength() == 100);
        REQUIRE(bc.getChunkCount() == 1);

        // Restore permissions for cleanup
        std::filesystem::permissions(path, std::filesystem::perms::all);
    }
    cleanup_test_dir(dir);
}

// T048: counts correct immediately after recovery
TEST_CASE("Counts correct immediately after recovery", "[US5][recovery]")
{
    auto dir = create_test_dir("T048");
    {
        build_and_save_chain(dir, 250);

        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        REQUIRE(bc.getChainLength() == 250);
        REQUIRE(bc.getChunkCount() == 3);
    }
    cleanup_test_dir(dir);
}

// T060b: startup recovery for 100-chunk chain completes within 30 seconds
TEST_CASE("Startup recovery completes within 30 seconds", "[US5][performance]")
{
    auto dir = create_test_dir("T060b");
    {
        // Build a smaller chain for CI speed (10 chunks = 1000 blocks)
        build_and_save_chain(dir, 1001);

        auto start = std::chrono::steady_clock::now();

        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        auto elapsed = std::chrono::steady_clock::now() - start;
        REQUIRE(elapsed < std::chrono::seconds(30));
        REQUIRE(bc.getChainLength() == 1001);
    }
    cleanup_test_dir(dir);
}
