#include <catch2/catch_all.hpp>
#include "../src/BlockPropagation.hpp"
#include "../src/PeerManager.hpp"
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/SyncState.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include "MockBlockchain.hpp"
#include <filesystem>

// Integration tests: BlockPropagation + MockBlockchain + PeerManager interactions

TEST_CASE("Integration: valid block received, validated, appended, relayed", "[integration][block_propagation]") {
    MockBlockchain bc;
    SyncStatus sync_status;

    bool relay_called = false;
    std::string relay_exclude;
    Block relayed_block;

    BlockPropagation bp(bc, sync_status, [&](const Block &block, const std::string &exclude) {
        relay_called = true;
        relay_exclude = exclude;
        relayed_block = block;
    });

    Block b = bc.createValidNextBlock("integration_test");

    bp.on_block_received(b, "node_a:9000");

    REQUIRE(bc.appended_blocks.size() == 1);
    REQUIRE(bc.appended_blocks[0].entries[0].data == "integration_test");
    REQUIRE(bc.save_chunk_called);
    REQUIRE(bc.save_keys_called);
    REQUIRE(relay_called);
    REQUIRE(relay_exclude == "node_a:9000");
    REQUIRE(relayed_block.hash == b.hash);
}

TEST_CASE("Integration: invalid block rejected, not relayed", "[integration][block_propagation]") {
    MockBlockchain bc;
    SyncStatus sync_status;

    bool relay_called = false;
    BlockPropagation bp(bc, sync_status, [&](const Block &, const std::string &) {
        relay_called = true;
    });

    // Create a block with wrong prevHash (will go to pending pool as gap block)
    StreamEntry bad_entry;
    bad_entry.stream = "test";
    bad_entry.key = "k";
    bad_entry.data = "bad_block";

    Block b;
    b.index = 1;
    b.timestamp = static_cast<uint64_t>(std::time(nullptr));
    b.entries = {bad_entry};
    b.prevHash = "nonexistent_hash";
    b.difficulty = 0;
    b.nonce = 0;
    b.hash = b.calculateHash();

    bp.on_block_received(b, "node_a:9000");

    REQUIRE(bc.appended_blocks.empty());
    REQUIRE_FALSE(relay_called);
}

TEST_CASE("Integration: duplicate block via dedup cache hit, discarded", "[integration][block_propagation]") {
    MockBlockchain bc;
    SyncStatus sync_status;

    int relay_count = 0;
    BlockPropagation bp(bc, sync_status, [&](const Block &, const std::string &) {
        relay_count++;
    });

    Block b = bc.createValidNextBlock("dedup_test");

    // First reception
    bp.on_block_received(b, "node_a:9000");
    REQUIRE(bc.appended_blocks.size() == 1);
    REQUIRE(relay_count == 1);

    // Second reception from different peer
    bp.on_block_received(b, "node_b:9000");
    REQUIRE(bc.appended_blocks.size() == 1); // No double append
    REQUIRE(relay_count == 1); // No double relay
}

TEST_CASE("Integration: gap block deferred, resolved when predecessor arrives", "[integration][block_propagation]") {
    MockBlockchain bc;
    SyncStatus sync_status;

    int relay_count = 0;
    BlockPropagation bp(bc, sync_status, [&](const Block &, const std::string &) {
        relay_count++;
    });

    // Build a chain: genesis -> b1 -> b2
    Block b1 = bc.createValidNextBlock("block_1");
    bc.blocks.push_back(b1);
    Block b2 = bc.createValidNextBlock("block_2");

    // Reset to just genesis
    bc.blocks.resize(1);
    bc.appended_blocks.clear();

    // Receive b2 first (gap block)
    bp.on_block_received(b2, "node_a:9000");
    REQUIRE(bc.appended_blocks.empty());

    // Receive b1 (fills the gap)
    bp.on_block_received(b1, "node_a:9000");

    // Both should now be appended: b1 directly, b2 via pending pool resolution
    REQUIRE(bc.appended_blocks.size() == 2);
    REQUIRE(bc.appended_blocks[0].entries[0].data == "block_1");
    REQUIRE(bc.appended_blocks[1].entries[0].data == "block_2");
    REQUIRE(relay_count == 2);
}

TEST_CASE("Integration: full pipeline with sync queue", "[integration][block_propagation]") {
    MockBlockchain bc;
    SyncStatus sync_status;
    sync_status.isSyncing.store(true);

    int relay_count = 0;
    BlockPropagation bp(bc, sync_status, [&](const Block &, const std::string &) {
        relay_count++;
    });

    Block b = bc.createValidNextBlock("during_sync");

    // Block arrives during sync
    bp.on_block_received(b, "node_a:9000");
    REQUIRE(bc.appended_blocks.empty());
    REQUIRE(relay_count == 0);

    // Sync completes
    sync_status.isSyncing.store(false);
    bp.process_sync_queue();

    REQUIRE(bc.appended_blocks.size() == 1);
    REQUIRE(relay_count == 1);
}

// T060c: Network integration - peer receives 101+ blocks, verifies chunk
// auto-save and recovery across restart using real Blockchain<Chunk>
TEST_CASE("Integration: 101+ blocks via appendBlock trigger chunk auto-save and survive recovery", "[integration][persistence]") {
    auto dir = std::filesystem::temp_directory_path() / "bp_integ_T060c";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    // Phase 1: Build a chain with >100 blocks and verify chunk file auto-saved
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.miningTimeout = 5;
        Blockchain<Chunk> bc(dir.string(), cfg);

        // Simulate receiving 105 blocks from a peer via appendBlock
        std::string prevHash = bc.getBlockByIndex(0).hash;
        for (int i = 1; i <= 105; i++) {
            StreamEntry e;
            e.stream = "net";
            e.key = "k" + std::to_string(i);
            e.data = "payload_" + std::to_string(i);

            Block b;
            b.index = static_cast<size_t>(i);
            b.timestamp = static_cast<uint64_t>(std::time(nullptr));
            b.entries = {e};
            b.prevHash = prevHash;
            b.difficulty = 0;
            b.nonce = 0;
            b.hash = b.calculateHash();
            prevHash = b.hash;

            bc.appendBlock(b);
        }

        REQUIRE(bc.getChainLength() == 106);
        REQUIRE(bc.getChunkCount() == 2);

        // chunk_000000.dat should have been auto-saved when it filled at 100 blocks
        REQUIRE(std::filesystem::exists(dir / "chunk_000000.dat"));

        // Save the active chunk before "shutdown"
        bc.saveAllChunks();
        REQUIRE(std::filesystem::exists(dir / "chunk_000001.dat"));
    }

    // Phase 2: Recover from disk (simulates daemon restart) and verify data
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.miningTimeout = 5;
        Blockchain<Chunk> bc2(dir.string(), cfg);
        bc2.recoverChain();

        REQUIRE(bc2.getChainLength() == 106);
        REQUIRE(bc2.getChunkCount() == 2);

        // Verify first and last blocks are intact
        Block first = bc2.getBlockByIndex(1);
        REQUIRE(first.entries[0].data == "payload_1");

        Block last = bc2.getBlockByIndex(105);
        REQUIRE(last.entries[0].data == "payload_105");

        // Verify a block across the chunk boundary
        Block boundary = bc2.getBlockByIndex(100);
        REQUIRE(boundary.entries[0].data == "payload_100");
    }

    std::filesystem::remove_all(dir);
}
