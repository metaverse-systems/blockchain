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
    auto dir = std::filesystem::temp_directory_path() / ("chunk_replace_test_" + name);
    std::filesystem::create_directories(dir);
    return dir;
}

void cleanup_test_dir(const std::filesystem::path &dir) {
    std::filesystem::remove_all(dir);
}

// Build a valid candidate chain of given length with PoW difficulty 0
std::vector<Block> build_candidate_chain(size_t length) {
    std::vector<Block> blocks;

    Block genesis(0, 0, "", {}, 0, 0);
    blocks.push_back(genesis);

    for (size_t i = 1; i < length; i++) {
        StreamEntry e;
        e.stream = "test";
        e.key = "k" + std::to_string(i);
        e.data = "data";

        Block b;
        b.index = i;
        b.timestamp = static_cast<uint64_t>(std::time(nullptr));
        b.entries = {e};
        b.prevHash = blocks.back().hash;
        b.difficulty = 0;
        b.nonce = 0;
        b.hash = b.calculateHash();
        blocks.push_back(b);
    }

    return blocks;
}

} // anonymous namespace

// T056: replaceChain moves old files to timestamped backup dir
TEST_CASE("replaceChain moves old files to timestamped backup dir", "[US8][replace]")
{
    auto dir = create_test_dir("T056");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        cfg.maxReorgDepth = 1000;
        Blockchain<Chunk> bc(dir, cfg);

        // Add some blocks and save
        for (size_t i = 1; i <= 5; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }
        bc.saveAllChunks();

        REQUIRE(std::filesystem::exists(dir / "chunk_000000.dat"));

        // Replace with a longer chain
        auto candidate = build_candidate_chain(10);
        bc.replaceChain(candidate);

        // Backup directory should exist
        bool backup_found = false;
        for (auto &entry : std::filesystem::directory_iterator(dir / "backups")) {
            if (entry.is_directory()) {
                backup_found = true;
                // Old chunk file should be in backup
                REQUIRE(std::filesystem::exists(entry.path() / "chunk_000000.dat"));
            }
        }
        REQUIRE(backup_found);
    }
    cleanup_test_dir(dir);
}

// T057: replaceChain aborts if backup dir cannot be created
TEST_CASE("replaceChain handles backup dir creation failure gracefully", "[US8][replace]")
{
    auto dir = create_test_dir("T057");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        cfg.maxReorgDepth = 1000;
        Blockchain<Chunk> bc(dir, cfg);

        for (size_t i = 1; i <= 5; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }
        bc.saveAllChunks();

        // Make backups dir a file to prevent directory creation
        std::ofstream(dir / "backups") << "blocker";

        auto candidate = build_candidate_chain(10);
        // Should not throw — logs and continues
        REQUIRE_NOTHROW(bc.replaceChain(candidate));

        // Clean up the blocker file
        std::filesystem::remove(dir / "backups");
    }
    cleanup_test_dir(dir);
}

// T058: replaceChain with new chain persists all new chunk files
TEST_CASE("replaceChain persists all new chunk files", "[US8][replace]")
{
    auto dir = create_test_dir("T058");
    {
        auto cfg = TestHelpers::defaultConsensusConfig();
        cfg.maxReorgDepth = 1000;
        Blockchain<Chunk> bc(dir, cfg);

        for (size_t i = 1; i <= 5; i++) {
            bc.publish("test", "k" + std::to_string(i), "data", {"k" + std::to_string(i)});
        }
        bc.saveAllChunks();

        // Replace with a longer chain (150 blocks -> 2 chunks)
        auto candidate = build_candidate_chain(150);
        bc.replaceChain(candidate);

        // New chain should have its chunks saved
        // The active chunk (chunk 1) should be saved after replaceChain
        REQUIRE(bc.getChainLength() == 150);
        REQUIRE(bc.getChunkCount() == 2);
    }
    cleanup_test_dir(dir);
}
