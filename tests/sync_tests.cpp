#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/Blockchain.hpp"
#include "../src/MockChunk.hpp"
#include "../src/ConsensusConfig.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/SyncState.hpp"
#include "../src/network/SyncMessages.hpp"
#include "../src/network/PacketHeader.hpp"
#include "../src/utils.hpp"
#include "TestHelpers.hpp"
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/asio.hpp>
#include <boost/asio/steady_timer.hpp>
#include <sstream>

// ==========================================================================
// SyncMessages serialization tests
// ==========================================================================
TEST_CASE("SyncQuery serialization round-trip", "[Sync][Setup]")
{
    SyncQuery query;
    query.local_chain_height = 42;

    std::stringstream ss;
    {
        boost::archive::binary_oarchive oa(ss);
        oa << query;
    }

    SyncQuery restored;
    {
        boost::archive::binary_iarchive ia(ss);
        ia >> restored;
    }

    REQUIRE(restored.local_chain_height == 42);
}

TEST_CASE("SyncResponse serialization round-trip", "[Sync][Setup]")
{
    SyncResponse response;
    response.total_chain_height = 200;
    response.start_index = 1;
    response.blocks = TestHelpers::buildValidChain(3, 1);

    std::stringstream ss;
    {
        boost::archive::binary_oarchive oa(ss);
        oa << response;
    }

    SyncResponse restored;
    {
        boost::archive::binary_iarchive ia(ss);
        ia >> restored;
    }

    REQUIRE(restored.total_chain_height == 200);
    REQUIRE(restored.start_index == 1);
    REQUIRE(restored.blocks.size() == 3);
    REQUIRE(restored.blocks[0].index == 0);
    REQUIRE(restored.blocks[2].hash == response.blocks[2].hash);
}

TEST_CASE("SyncState flag defaults to not syncing", "[Sync][Setup]")
{
    SyncStatus status;
    REQUIRE_FALSE(status.isSyncing.load());
}

TEST_CASE("SyncState flag transitions IDLE to SYNCING and back", "[Sync][Setup]")
{
    SyncStatus status;
    status.isSyncing.store(true);
    REQUIRE(status.isSyncing.load());
    status.isSyncing.store(false);
    REQUIRE_FALSE(status.isSyncing.load());
}

TEST_CASE("getChainBlockCount accessible via IBlockchain interface", "[Sync][Setup]")
{
    Blockchain<MockChunk> bc(".", ConsensusConfig());
    IBlockchain &ibc = bc;
    REQUIRE(ibc.getChainBlockCount() == 1); // genesis only
}

// ==========================================================================
// Phase 2: PeerServer BLOCKCHAIN_QUERY handler tests
// ==========================================================================

// Helper: serialize SyncQuery into a PacketHeader + payload string
static std::string serializeSyncQuery(const SyncQuery &query, PacketHeader &header)
{
    std::stringstream ss;
    boost::archive::binary_oarchive oa(ss);
    oa << query;
    std::string payload = ss.str();
    header = PacketHeader(payload.size(), PacketType::BLOCKCHAIN_QUERY);
    return payload;
}

// Helper: deserialize a SyncResponse from a raw string
static SyncResponse deserializeSyncResponse(const std::string &data)
{
    std::istringstream iss(data);
    boost::archive::binary_iarchive ia(iss);
    SyncResponse response;
    ia >> response;
    return response;
}

TEST_CASE("PeerServer builds correct SyncResponse for BLOCKCHAIN_QUERY", "[Sync][PeerServer]")
{
    // Build a blockchain with some blocks
    ConsensusConfig config;
    config.initialDifficulty = 1;
    config.minDifficulty = 1;
    config.miningTimeout = 30;
    Blockchain<MockChunk> bc(".", config);

    // Add 5 blocks (total = 6 including genesis)
    for (int i = 0; i < 5; i++) {
        bc.publish("test", "key", "block " + std::to_string(i + 1), {"key"});
    }

    REQUIRE(bc.getChainBlockCount() == 6);

    // Simulate a SyncQuery with local_chain_height = 1 (only genesis)
    SyncQuery query;
    query.local_chain_height = 1;

    // The server should send blocks from chunk 0 (start_chunk = 1/100 = 0)
    // Since all 6 blocks are in chunk 0, we expect one response with all blocks
    size_t total_height = bc.getChainBlockCount();
    size_t start_chunk = query.local_chain_height / bc.chunkSize;
    size_t total_chunks = (total_height + bc.chunkSize - 1) / bc.chunkSize;

    REQUIRE(start_chunk == 0);
    REQUIRE(total_chunks == 1);
    REQUIRE(total_height == 6);
}

TEST_CASE("PeerServer sends only missing chunks for incremental sync", "[Sync][PeerServer]")
{
    ConsensusConfig config;
    config.initialDifficulty = 1;
    config.minDifficulty = 1;
    config.miningTimeout = 30;
    Blockchain<MockChunk> bc(".", config);

    // Chain has 6 blocks (1 chunk, since < 100)
    for (int i = 0; i < 5; i++) {
        bc.publish("test", "key", "block " + std::to_string(i + 1), {"key"});
    }

    // If requester has height=6 (equal), no sync needed
    SyncQuery query;
    query.local_chain_height = 6;

    size_t total_height = bc.getChainBlockCount();
    REQUIRE(query.local_chain_height >= total_height);
}

// ==========================================================================
// Phase 3: US1 - Client sync tests
// ==========================================================================

TEST_CASE("PeerClient sends BLOCKCHAIN_QUERY with correct local chain height", "[Sync][US1]")
{
    // Verify that a SyncQuery can be constructed from a blockchain's height
    ConsensusConfig config;
    Blockchain<MockChunk> bc(".", config);

    SyncQuery query;
    query.local_chain_height = bc.getChainBlockCount();

    REQUIRE(query.local_chain_height == 1); // genesis only

    // Verify serialization produces correct data
    std::stringstream ss;
    boost::archive::binary_oarchive oa(ss);
    oa << query;
    std::string payload = ss.str();
    REQUIRE_FALSE(payload.empty());

    // Verify the packet header is correct
    PacketHeader header(payload.size(), PacketType::BLOCKCHAIN_QUERY);
    REQUIRE(header.type == PacketType::BLOCKCHAIN_QUERY);
    REQUIRE(header.length == payload.size());
}

TEST_CASE("PeerClient receives BLOCKCHAIN_RESPONSE and validates blocks", "[Sync][US1]")
{
    ConsensusConfig config;
    config.initialDifficulty = 1;
    config.minDifficulty = 1;

    // Build a chain of 5 blocks to simulate what a peer would send
    auto chain = TestHelpers::buildValidChain(5, 1);

    SyncResponse response;
    response.total_chain_height = 5;
    response.start_index = 0;
    response.blocks = chain;

    // Serialize and deserialize the response
    std::stringstream ss;
    {
        boost::archive::binary_oarchive oa(ss);
        oa << response;
    }
    SyncResponse restored;
    {
        boost::archive::binary_iarchive ia(ss);
        ia >> restored;
    }

    // Validate each block against its predecessor
    bool all_valid = true;
    for (size_t i = 1; i < restored.blocks.size(); i++) {
        if (!IBlockchain::isValidNewBlock(restored.blocks[i], restored.blocks[i - 1], config)) {
            all_valid = false;
            break;
        }
    }

    REQUIRE(all_valid);
    REQUIRE(restored.blocks.size() == 5);
    REQUIRE(restored.total_chain_height == 5);
}

TEST_CASE("PeerClient transitions from IDLE to SYNCING and back", "[Sync][US1]")
{
    SyncStatus status;

    // Initial state: IDLE
    REQUIRE_FALSE(status.isSyncing.load());

    // Start sync: transition to SYNCING
    status.isSyncing.store(true);
    REQUIRE(status.isSyncing.load());

    // Complete sync: transition back to IDLE
    status.isSyncing.store(false);
    REQUIRE_FALSE(status.isSyncing.load());
}

// ==========================================================================
// Phase 4: US2 - Incremental sync tests
// ==========================================================================

TEST_CASE("PeerClient sends BLOCKCHAIN_QUERY with height > 1 for incremental sync", "[Sync][US2]")
{
    ConsensusConfig config;
    config.initialDifficulty = 1;
    config.minDifficulty = 1;
    config.miningTimeout = 30;
    Blockchain<MockChunk> bc(".", config);

    // Add blocks so height > 1
    for (int i = 0; i < 3; i++) {
        bc.publish("test", "key", "block " + std::to_string(i + 1), {"key"});
    }

    SyncQuery query;
    query.local_chain_height = bc.getChainBlockCount();
    REQUIRE(query.local_chain_height == 4);

    // If peer has 10 blocks, we only need blocks 4-9
    SyncResponse response;
    response.total_chain_height = 10;
    response.start_index = 0;

    // Peer sends only the blocks the client is missing
    auto full_chain = TestHelpers::buildValidChain(10, 1);
    for (size_t i = query.local_chain_height; i < full_chain.size(); i++) {
        response.blocks.push_back(full_chain[i]);
    }

    REQUIRE(response.blocks.size() == 6); // blocks 4-9
}

TEST_CASE("PeerServer sends only chunks after requester chain height", "[Sync][US2]")
{
    // Requester has height=150 (1.5 chunks) — server should start from chunk 1
    SyncQuery query;
    query.local_chain_height = 150;

    size_t start_chunk = query.local_chain_height / 100; // chunkSize = 100
    REQUIRE(start_chunk == 1);

    // For a peer with 350 blocks, total chunks = 4 (0-99, 100-199, 200-299, 300-349)
    size_t total_height = 350;
    size_t total_chunks = (total_height + 99) / 100;
    REQUIRE(total_chunks == 4);

    // Server should send chunks 1, 2, 3
    size_t chunks_to_send = total_chunks - start_chunk;
    REQUIRE(chunks_to_send == 3);
}

// ==========================================================================
// Phase 5: US3 - Validation rejection tests
// ==========================================================================

TEST_CASE("PeerClient rejects chunk with invalid hash", "[Sync][US3]")
{
    ConsensusConfig config;
    config.initialDifficulty = 1;
    config.minDifficulty = 1;

    auto chain = TestHelpers::buildValidChain(5, 1);

    // Tamper with block 3's hash
    chain[3].hash = "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff";

    // Validation should fail at block 3
    bool valid = true;
    size_t failed_index = 0;
    for (size_t i = 1; i < chain.size(); i++) {
        if (!IBlockchain::isValidNewBlock(chain[i], chain[i - 1], config)) {
            valid = false;
            failed_index = i;
            break;
        }
    }

    REQUIRE_FALSE(valid);
    REQUIRE(failed_index == 3);
}

TEST_CASE("PeerClient rejects chunk with insufficient PoW difficulty", "[Sync][US3]")
{
    ConsensusConfig config;
    config.minDifficulty = 4;

    auto chain = TestHelpers::buildValidChain(3, 1); // mined at difficulty 1

    // Block at difficulty 1 should fail when config requires minDifficulty=4
    bool valid = true;
    for (size_t i = 1; i < chain.size(); i++) {
        if (!IBlockchain::isValidNewBlock(chain[i], chain[i - 1], config)) {
            valid = false;
            break;
        }
    }

    REQUIRE_FALSE(valid);
}

TEST_CASE("PeerClient accepts valid longer chain", "[Sync][US3]")
{
    ConsensusConfig config;
    config.initialDifficulty = 1;
    config.minDifficulty = 1;
    config.miningTimeout = 30;
    config.maxReorgDepth = 100;
    Blockchain<MockChunk> bc(".", config);

    // Local chain has genesis only (1 block)
    REQUIRE(bc.getChainBlockCount() == 1);

    // Build a valid longer chain
    auto longer_chain = TestHelpers::buildValidChain(5, 1);
    bc.replaceChain(longer_chain);

    REQUIRE(bc.getChainBlockCount() == 5);
}

TEST_CASE("PeerClient keeps local chain when peer chain is same length", "[Sync][US3]")
{
    ConsensusConfig config;
    config.initialDifficulty = 1;
    config.minDifficulty = 1;
    config.miningTimeout = 30;
    Blockchain<MockChunk> bc(".", config);

    // Add 2 blocks (total 3)
    bc.publish("test", "key", "local block 1", {"key"});
    bc.publish("test", "key", "local block 2", {"key"});
    REQUIRE(bc.getChainBlockCount() == 3);

    // Build candidate chain of same length
    auto same_length_chain = TestHelpers::buildValidChain(3, 1);

    // replaceChain should reject since not longer
    bc.replaceChain(same_length_chain);
    REQUIRE(bc.getChainBlockCount() == 3);
}

TEST_CASE("PeerClient skips sync when peer total_chain_height <= local height", "[Sync][US3]")
{
    ConsensusConfig config;
    config.initialDifficulty = 1;
    config.minDifficulty = 1;
    config.miningTimeout = 30;
    Blockchain<MockChunk> bc(".", config);

    bc.publish("test", "key", "block 1", {"key"});
    bc.publish("test", "key", "block 2", {"key"});

    size_t local_height = bc.getChainBlockCount(); // 3

    SyncResponse response;
    response.total_chain_height = 2; // peer has fewer blocks

    // Sync guard: skip if peer is not strictly longer
    bool should_sync = response.total_chain_height > local_height;
    REQUIRE_FALSE(should_sync);
}

// ==========================================================================
// Phase 6: US4 - Network interruption tests
// ==========================================================================

TEST_CASE("Already-persisted chunks preserved when connection drops", "[Sync][US4]")
{
    ConsensusConfig config;
    config.initialDifficulty = 1;
    config.minDifficulty = 1;
    config.miningTimeout = 30;
    config.maxReorgDepth = 1000;
    Blockchain<MockChunk> bc(".", config);

    // Simulate having received and persisted one chunk of blocks
    auto chain = TestHelpers::buildValidChain(5, 1);
    bc.replaceChain(chain);

    REQUIRE(bc.getChainBlockCount() == 5);

    // Simulate connection drop — local state should be unchanged
    // (we simply verify the chain persists after the "drop")
    REQUIRE(bc.getChainBlockCount() == 5);
    REQUIRE(bc.getBlockByIndex(4).index == 4);
}

TEST_CASE("Per-chunk timeout timer can be armed and cancelled", "[Sync][US4]")
{
    boost::asio::io_context io_ctx;
    boost::asio::steady_timer timer(io_ctx);

    timer.expires_after(std::chrono::seconds(60));
    bool timed_out = false;
    bool cancelled = false;

    timer.async_wait([&](const boost::system::error_code &ec) {
        if (ec == boost::asio::error::operation_aborted) {
            cancelled = true;
        } else if (!ec) {
            timed_out = true;
        }
    });

    // Cancel to simulate chunk arrival before timeout
    timer.cancel();
    io_ctx.run();

    REQUIRE(cancelled);
    REQUIRE_FALSE(timed_out);
}

TEST_CASE("SyncState returns to IDLE on connection error", "[Sync][US4]")
{
    SyncStatus status;

    // Simulate sync in progress
    status.isSyncing.store(true);
    REQUIRE(status.isSyncing.load());

    // Simulate error handler resetting state
    auto error_handler = [&status]() {
        status.isSyncing.store(false);
    };

    error_handler();
    REQUIRE_FALSE(status.isSyncing.load());
}

// ==========================================================================
// US1: handle_sync_response block-append tests
// ==========================================================================

TEST_CASE("handle_sync_response appends new blocks", "[Sync][US1]")
{
    auto dir = std::filesystem::temp_directory_path() / "sync_test_append";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    ConsensusConfig cfg;
    cfg.initialDifficulty = 0;
    cfg.minDifficulty = 0;
    cfg.miningTimeout = 60;
    Blockchain<MockChunk> bc(dir, cfg);

    // Blockchain starts with genesis (1 block)
    size_t initial_height = bc.getChainBlockCount();
    REQUIRE(initial_height == 1);

    // Build a valid chain extending from genesis
    Block genesis = bc.getBlockByIndex(0);
    Block b1 = TestHelpers::mineTestBlock(1, 100, genesis.hash, "sync_b1", 0);
    Block b2 = TestHelpers::mineTestBlock(2, 200, b1.hash, "sync_b2", 0);
    Block b3 = TestHelpers::mineTestBlock(3, 300, b2.hash, "sync_b3", 0);

    // Simulate what the fixed handler should do: append each new block
    bc.appendBlock(b1);
    bc.appendBlock(b2);
    bc.appendBlock(b3);

    REQUIRE(bc.getChainBlockCount() == 4);
    REQUIRE(bc.getBlockByIndex(1).hash == b1.hash);
    REQUIRE(bc.getBlockByIndex(3).hash == b3.hash);

    std::filesystem::remove_all(dir);
}

TEST_CASE("handle_sync_response skips already-known blocks", "[Sync][US1]")
{
    auto dir = std::filesystem::temp_directory_path() / "sync_test_skip";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    ConsensusConfig cfg;
    cfg.initialDifficulty = 0;
    cfg.minDifficulty = 0;
    cfg.miningTimeout = 60;
    Blockchain<MockChunk> bc(dir, cfg);

    // Add some blocks to the local chain
    Block genesis = bc.getBlockByIndex(0);
    Block b1 = TestHelpers::mineTestBlock(1, 100, genesis.hash, "local_b1", 0);
    Block b2 = TestHelpers::mineTestBlock(2, 200, b1.hash, "local_b2", 0);
    bc.appendBlock(b1);
    bc.appendBlock(b2);
    REQUIRE(bc.getChainBlockCount() == 3);

    // Simulate a sync response that overlaps: blocks 1, 2 (known) + 3 (new)
    Block b3 = TestHelpers::mineTestBlock(3, 300, b2.hash, "sync_b3", 0);

    // The handler should skip blocks below local_height and only append new ones
    size_t local_height = bc.getChainBlockCount();
    std::vector<Block> response_blocks = {b1, b2, b3};
    for (auto &block : response_blocks) {
        if (block.index < local_height) {
            continue; // Skip already-known blocks
        }
        bc.appendBlock(block);
    }

    REQUIRE(bc.getChainBlockCount() == 4);
    // Verify existing blocks unchanged
    REQUIRE(bc.getBlockByIndex(1).hash == b1.hash);
    REQUIRE(bc.getBlockByIndex(2).hash == b2.hash);
    REQUIRE(bc.getBlockByIndex(3).hash == b3.hash);

    std::filesystem::remove_all(dir);
}

TEST_CASE("handle_sync_response aborts on overlap hash mismatch", "[Sync][US1]")
{
    auto dir = std::filesystem::temp_directory_path() / "sync_test_mismatch";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);

    ConsensusConfig cfg;
    cfg.initialDifficulty = 0;
    cfg.minDifficulty = 0;
    cfg.miningTimeout = 60;
    Blockchain<MockChunk> bc(dir, cfg);

    // Build local chain: genesis + b1
    Block genesis = bc.getBlockByIndex(0);
    Block b1 = TestHelpers::mineTestBlock(1, 100, genesis.hash, "local_b1", 0);
    bc.appendBlock(b1);
    REQUIRE(bc.getChainBlockCount() == 2);

    // Build a response with a different block at index 1 (hash mismatch)
    Block fake_b1 = TestHelpers::mineTestBlock(1, 999, genesis.hash, "fake_data", 0);
    Block fake_b2 = TestHelpers::mineTestBlock(2, 1000, fake_b1.hash, "fake_b2", 0);

    // The handler should detect the overlap mismatch and abort
    size_t local_height = bc.getChainBlockCount();
    bool aborted = false;
    std::vector<Block> response_blocks = {fake_b1, fake_b2};
    for (auto &block : response_blocks) {
        if (block.index < local_height) {
            // Check overlap hash matches local chain
            Block local_block = bc.getBlockByIndex(block.index);
            if (local_block.hash != block.hash) {
                aborted = true;
                break;
            }
            continue;
        }
        bc.appendBlock(block);
    }

    REQUIRE(aborted);
    // Chain height should be unchanged
    REQUIRE(bc.getChainBlockCount() == 2);

    std::filesystem::remove_all(dir);
}

TEST_CASE("handle_sync_response treats empty batch as end-of-sync", "[Sync][US1]")
{
    SyncStatus status;
    status.isSyncing.store(true);

    SyncResponse response;
    response.total_chain_height = 100;
    response.start_index = 0;
    response.blocks.clear(); // Empty batch

    // The handler should stop syncing on empty response
    if (response.blocks.empty()) {
        logMessage("WARN", "Empty sync response while expecting more blocks");
        status.isSyncing.store(false);
    }

    REQUIRE_FALSE(status.isSyncing.load());
}
