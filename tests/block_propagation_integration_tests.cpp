#include <catch2/catch_all.hpp>
#include "../src/BlockPropagation.hpp"
#include "../src/PeerManager.hpp"
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/SyncState.hpp"
#include "MockBlockchain.hpp"

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
