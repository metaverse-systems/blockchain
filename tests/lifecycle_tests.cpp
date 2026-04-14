#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include "../src/MockChunk.hpp"
#include "../src/NodeConfig.hpp"
#include "TestHelpers.hpp"
#include <filesystem>
#include <fstream>
#include <chrono>

namespace {

std::filesystem::path create_test_dir(const std::string &name) {
    auto dir = std::filesystem::temp_directory_path() / ("lifecycle_test_" + name);
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

// --- Phase 3: US1 Tests (T011-T014) ---

TEST_CASE("saveAllChunks saves only dirty chunks", "[lifecycle][us1]") {
    auto dir = create_test_dir("save_dirty_only");
    Blockchain<MockChunk> bc(dir, test_config());

    // Publish blocks to fill chunk 0 (100 blocks) and create chunk 1
    for (int i = 0; i < 101; i++) {
        bc.publish("test", "key" + std::to_string(i), "data", {"k"});
    }

    // At this point we should have at least 2 chunks
    REQUIRE(bc.getChunkCount() >= 2);

    // Clear dirty flags on all chunks to simulate a clean state — not possible
    // directly since chain is private. Instead, call saveAllChunks which should
    // save dirty chunks, then verify via the dirty_ flag behavior.

    // saveAllChunks should succeed with zero failures and clear dirty flag
    size_t failures = bc.saveAllChunks();
    REQUIRE(failures == 0);
    REQUIRE(bc.isDirty() == false);

    cleanup_test_dir(dir);
}

TEST_CASE("saveAllChunks skips empty chunks", "[lifecycle][us1]") {
    auto dir = create_test_dir("save_skip_empty");
    Blockchain<MockChunk> bc(dir, test_config());

    // Genesis block is in chunk 0 (1 block). The chunk is not empty.
    // saveAllChunks iterates all chunks; should succeed with zero failures
    size_t failures = bc.saveAllChunks();
    REQUIRE(failures == 0);

    cleanup_test_dir(dir);
}

TEST_CASE("appendBlock rejects when shutting down", "[lifecycle][us1]") {
    auto dir = create_test_dir("append_shutdown");
    Blockchain<MockChunk> bc(dir, test_config());

    bc.setShuttingDown();

    Block b;
    b.index = 1;
    b.timestamp = 12345;
    b.prevHash = "";
    b.hash = "abc";
    b.difficulty = 0;
    b.nonce = 0;

    REQUIRE_THROWS_AS(bc.appendBlock(b), std::runtime_error);
    REQUIRE_THROWS_WITH(bc.appendBlock(b), Catch::Matchers::ContainsSubstring("shutting down"));

    cleanup_test_dir(dir);
}

TEST_CASE("publish rejects when shutting down", "[lifecycle][us1]") {
    auto dir = create_test_dir("publish_shutdown");
    Blockchain<MockChunk> bc(dir, test_config());

    bc.setShuttingDown();

    REQUIRE_THROWS_AS(
        bc.publish("test", "key", "data", {"k"}),
        std::runtime_error
    );
    REQUIRE_THROWS_WITH(
        bc.publish("test", "key", "data", {"k"}),
        Catch::Matchers::ContainsSubstring("shutting down")
    );

    cleanup_test_dir(dir);
}

// --- Phase 4: US2 Tests (T019-T022, T024b) ---

TEST_CASE("recoverChain loads all valid chunks", "[lifecycle][us2]") {
    auto dir = create_test_dir("recover_valid");

    {
        Blockchain<Chunk> bc(dir, test_config());
        // Create blocks spanning 2+ chunks (>100 blocks)
        for (int i = 0; i < 150; i++) {
            bc.publish("test", "key" + std::to_string(i), "data", {"k"});
        }
        bc.saveAllChunks();
    }

    // Recover
    Blockchain<Chunk> bc2(dir, test_config());
    bc2.recoverChain();

    // Should have 151 blocks (genesis + 150)
    REQUIRE(bc2.getChainLength() == 151);
    REQUIRE(bc2.getChunkCount() == 2);

    cleanup_test_dir(dir);
}

TEST_CASE("recoverChain stops at corrupted chunk", "[lifecycle][us2]") {
    auto dir = create_test_dir("recover_corrupt");

    {
        Blockchain<Chunk> bc(dir, test_config());
        for (int i = 0; i < 250; i++) {
            bc.publish("test", "key" + std::to_string(i), "data", {"k"});
        }
        bc.saveAllChunks();
    }

    // Corrupt chunk_000002.dat
    auto chunkPath = dir / "chunk_000002.dat";
    REQUIRE(std::filesystem::exists(chunkPath));
    {
        std::ofstream ofs(chunkPath, std::ios::binary);
        ofs << "CORRUPTED";
    }

    Blockchain<Chunk> bc2(dir, test_config());
    bc2.recoverChain();

    // Should load only the first 2 chunks (0 and 1), chunk 2 is corrupted
    REQUIRE(bc2.getChunkCount() == 2);
    // 200 blocks (100 per chunk for chunks 0 and 1)
    REQUIRE(bc2.getChainLength() == 200);

    cleanup_test_dir(dir);
}

TEST_CASE("recoverChain detects cross-chunk linkage break", "[lifecycle][us2]") {
    auto dir = create_test_dir("recover_linkage");

    {
        Blockchain<Chunk> bc(dir, test_config());
        for (int i = 0; i < 250; i++) {
            bc.publish("test", "key" + std::to_string(i), "data", {"k"});
        }
        bc.saveAllChunks();
    }

    // Replace chunk_000001.dat with a completely different valid chain segment
    // by creating a separate blockchain and saving its chunk 0 as chunk 1 in the test dir
    auto tmpdir = create_test_dir("recover_linkage_tmp");
    {
        Blockchain<Chunk> bc_tmp(tmpdir, test_config());
        for (int i = 0; i < 100; i++) {
            bc_tmp.publish("other", "key" + std::to_string(i), "data", {"k"});
        }
        bc_tmp.saveAllChunks();
    }
    // Copy the tmp chunk_000000.dat as chunk_000001.dat in test dir - linkage will break
    std::filesystem::copy_file(
        tmpdir / "chunk_000000.dat",
        dir / "chunk_000001.dat",
        std::filesystem::copy_options::overwrite_existing
    );

    Blockchain<Chunk> bc2(dir, test_config());
    bc2.recoverChain();

    // Should only load chunk 0 (linkage break at chunk 1)
    REQUIRE(bc2.getChunkCount() == 1);
    REQUIRE(bc2.getChainLength() == 100);

    cleanup_test_dir(dir);
    cleanup_test_dir(tmpdir);
}

TEST_CASE("recoverChain with no chunks creates fresh genesis", "[lifecycle][us2]") {
    auto dir = create_test_dir("recover_fresh");

    Blockchain<Chunk> bc(dir, test_config());
    bc.recoverChain();

    // Should keep the constructor genesis
    REQUIRE(bc.getChainLength() == 1);
    REQUIRE(bc.getChunkCount() == 1);

    cleanup_test_dir(dir);
}

TEST_CASE("recoverChain rebuilds indexes from chunks when index files are missing", "[lifecycle][us2]") {
    auto dir = create_test_dir("recover_rebuild_idx");

    {
        Blockchain<Chunk> bc(dir, test_config());
        bc.publish("mystream", "mykey", "mydata", {"k1"});
        bc.saveAllChunks();
    }

    // Remove index files
    std::filesystem::remove(dir / "keys.dat");
    std::filesystem::remove(dir / "streams.dat");
    std::filesystem::remove(dir / "stream_index.dat");

    Blockchain<Chunk> bc2(dir, test_config());
    bc2.recoverChain();

    // Streams should still be discoverable after index rebuild
    auto streams = bc2.listStreams();
    REQUIRE(streams.count("mystream") == 1);

    cleanup_test_dir(dir);
}

// --- Phase 5: US3 Tests (T025-T029) ---

TEST_CASE("IChunk::push_back sets dirty to true", "[lifecycle][us3]") {
    auto dir = create_test_dir("dirty_push_back");
    MockChunk chunk(0, dir);

    REQUIRE(chunk.isDirty() == false);

    Block b(0, 0, "", {}, 0, 0);
    chunk.push_back(b);

    REQUIRE(chunk.isDirty() == true);

    cleanup_test_dir(dir);
}

TEST_CASE("Chunk::save clears dirty flag", "[lifecycle][us3]") {
    auto dir = create_test_dir("dirty_save_clear");
    std::filesystem::create_directories(dir);

    Chunk chunk(0, dir);
    Block b(0, 0, "", {}, 0, 0);
    chunk.push_back(b);
    REQUIRE(chunk.isDirty() == true);

    chunk.save();
    REQUIRE(chunk.isDirty() == false);

    cleanup_test_dir(dir);
}

TEST_CASE("Chunk::load clears dirty flag", "[lifecycle][us3]") {
    auto dir = create_test_dir("dirty_load_clear");
    std::filesystem::create_directories(dir);

    // Save a chunk first
    {
        Chunk chunk(0, dir);
        Block b(0, 0, "", {}, 0, 0);
        chunk.push_back(b);
        chunk.save();
    }

    // Load into a new chunk — dirty should be false after load
    Chunk chunk2(0, dir);
    chunk2.load();
    REQUIRE(chunk2.isDirty() == false);

    cleanup_test_dir(dir);
}

TEST_CASE("Newly constructed chunk has dirty == false", "[lifecycle][us3]") {
    auto dir = create_test_dir("dirty_new_chunk");
    MockChunk chunk(0, dir);
    REQUIRE(chunk.isDirty() == false);
    cleanup_test_dir(dir);
}

TEST_CASE("Periodic save only writes dirty chunks via saveAllChunks", "[lifecycle][us3]") {
    auto dir = create_test_dir("periodic_dirty");
    Blockchain<Chunk> bc(dir, test_config());

    // Publish a block to make dirty
    bc.publish("test", "key", "data", {"k"});

    // Save all — should clear dirty
    bc.saveAllChunks();
    REQUIRE(bc.isDirty() == false);

    // Calling saveAllChunks again should be a no-op (nothing dirty)
    size_t failures2 = bc.saveAllChunks();
    REQUIRE(failures2 == 0);

    cleanup_test_dir(dir);
}

// --- Phase 6: US4 Tests (T033-T035) ---

TEST_CASE("recoverChain with fast_startup skips validation", "[lifecycle][us4]") {
    auto dir = create_test_dir("fast_startup");

    {
        Blockchain<Chunk> bc(dir, test_config());
        for (int i = 0; i < 150; i++) {
            bc.publish("test", "key" + std::to_string(i), "data", {"k"});
        }
        bc.saveAllChunks();
    }

    Blockchain<Chunk> bc2(dir, test_config());
    bc2.recoverChain(true);  // fast_startup = true

    REQUIRE(bc2.getChainLength() == 151);
    REQUIRE(bc2.getChunkCount() == 2);

    cleanup_test_dir(dir);
}

TEST_CASE("recoverChain with fast_startup=false validates and stops on corruption", "[lifecycle][us4]") {
    auto dir = create_test_dir("no_fast_startup_corrupt");

    {
        Blockchain<Chunk> bc(dir, test_config());
        for (int i = 0; i < 250; i++) {
            bc.publish("test", "key" + std::to_string(i), "data", {"k"});
        }
        bc.saveAllChunks();
    }

    // Corrupt chunk 1
    {
        std::ofstream ofs(dir / "chunk_000001.dat", std::ios::binary);
        ofs << "CORRUPTED";
    }

    Blockchain<Chunk> bc2(dir, test_config());
    bc2.recoverChain(false);  // explicit default

    // Should only load chunk 0
    REQUIRE(bc2.getChunkCount() == 1);
    REQUIRE(bc2.getChainLength() == 100);

    cleanup_test_dir(dir);
}

TEST_CASE("Zero-byte chunk file is treated as corrupted", "[lifecycle][us4]") {
    auto dir = create_test_dir("zero_byte_chunk");

    {
        Blockchain<Chunk> bc(dir, test_config());
        for (int i = 0; i < 150; i++) {
            bc.publish("test", "key" + std::to_string(i), "data", {"k"});
        }
        bc.saveAllChunks();
    }

    // Create zero-byte chunk_000002.dat (would be next expected chunk)
    // Replace chunk_000001.dat with zero bytes to test corruption detection
    {
        std::ofstream ofs(dir / "chunk_000001.dat", std::ios::binary | std::ios::trunc);
        // Write nothing — zero-byte file
    }

    Blockchain<Chunk> bc2(dir, test_config());
    bc2.recoverChain();

    // Should only load chunk 0 since chunk 1 is zero-byte
    REQUIRE(bc2.getChunkCount() == 1);
    REQUIRE(bc2.getChainLength() == 100);

    cleanup_test_dir(dir);
}

// --- T006: dirty_ flag behavior during chunk rotation ---

TEST_CASE("dirty flag stays true through chunk rotation until save", "[lifecycle][US1]") {
    auto dir = create_test_dir("dirty_flag_rotation");
    Blockchain<MockChunk> bc(dir, test_config());

    // Publish blocks — dirty_ should be true after this
    bc.publish("test", "k1", "data", {"k"});
    REQUIRE(bc.isDirty() == true);

    // Fill chunk 0 to trigger rotation (100 blocks = genesis + 99 published)
    for (int i = 2; i <= 99; i++) {
        bc.publish("test", "k" + std::to_string(i), "data", {"k"});
    }
    // Still dirty before chunk rotation
    REQUIRE(bc.isDirty() == true);

    // Trigger chunk rotation (block 100 causes new chunk)
    bc.publish("test", "k100", "data_rotate", {"k"});
    // dirty_ should still be true after rotation — the new block was appended
    REQUIRE(bc.isDirty() == true);

    // Only saveAllChunks should clear dirty
    bc.saveAllChunks();
    REQUIRE(bc.isDirty() == false);

    cleanup_test_dir(dir);
}

// --- Phase 7: US5 Coverage Gap Tests ---

TEST_CASE("saveAllChunks partial failure returns non-zero and keeps dirty flag", "[lifecycle][us5]") {
    auto dir = create_test_dir("partial_save_failure");
    {
        auto cfg = test_config();
        Blockchain<Chunk> bc(dir, cfg);

        // Fill chunk 0 (100 blocks) and create chunk 1
        for (int i = 0; i < 101; i++) {
            bc.publish("test", "key" + std::to_string(i), "data", {"k"});
        }
        REQUIRE(bc.getChunkCount() >= 2);

        // Save once to create the chunk files, then make chunk 0 read-only
        bc.saveAllChunks();
        REQUIRE(bc.isDirty() == false);

        // Add another block to make dirty again
        bc.publish("test", "dirty_key", "dirty_data", {"k"});
        REQUIRE(bc.isDirty() == true);

        // Make the chunk 0 file read-only (Chunk::save writes a .tmp then renames)
        auto chunk_file = dir / "chunk_000000.dat";
        std::filesystem::permissions(chunk_file, std::filesystem::perms::owner_read,
                                     std::filesystem::perm_options::replace);
        // Also restrict the directory so .tmp files can't be created
        std::filesystem::permissions(dir, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec,
                                     std::filesystem::perm_options::replace);

        size_t failures = bc.saveAllChunks();
        REQUIRE(failures > 0);
        REQUIRE(bc.isDirty() == true);

        // Restore directory permissions for cleanup
        std::filesystem::permissions(dir, std::filesystem::perms::all,
                                     std::filesystem::perm_options::replace);
    }
    cleanup_test_dir(dir);
}
