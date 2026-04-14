#include <catch2/catch_all.hpp>
#include "../src/ChainService.hpp"
#include "../src/ChainError.hpp"
#include "../src/Block.hpp"
#include "../src/SyncState.hpp"
#include "../src/MerkleTree.hpp"
#include "../src/utils.hpp"
#include "MockBlockchain.hpp"
#include "TestHelpers.hpp"
#include <sstream>
#include <boost/archive/binary_oarchive.hpp>

TEST_CASE("ChainService submitBlock validates then persists", "[chain_service]") {
    MockBlockchain bc;
    ChainService svc(bc);

    Block b = bc.createValidNextBlock("test_submit");
    svc.submitBlock(b);

    REQUIRE(bc.appended_blocks.size() == 1);
    REQUIRE(bc.appended_blocks[0].hash == b.hash);
    REQUIRE(bc.save_chunk_called);
    REQUIRE(bc.save_keys_called);
}

TEST_CASE("ChainService submitBlock throws ValidationError on invalid block", "[chain_service]") {
    MockBlockchain bc;
    ChainService svc(bc);

    Block bad;
    bad.index = 1;
    bad.timestamp = 0;
    bad.prevHash = "wrong_hash";
    bad.hash = "invalid";
    bad.difficulty = 1;
    bad.nonce = 0;

    REQUIRE_THROWS_AS(svc.submitBlock(bad), ValidationError);
    REQUIRE(bc.appended_blocks.empty());
}

TEST_CASE("ChainService submitSyncBatch handles overlap and appends new blocks", "[chain_service]") {
    MockBlockchain bc;
    ChainService svc(bc);

    // Add a block to the chain first
    Block b1 = bc.createValidNextBlock("sync1");
    bc.appendBlock(b1);

    // Create sync batch that overlaps with existing block and adds a new one
    Block b2 = bc.createValidNextBlock("sync2");

    std::vector<Block> batch = {b1, b2};

    // b1 already in chain, b2 new — should overlap-verify b1 and append b2
    bc.appended_blocks.clear();
    bc.save_chunk_called = false;
    bc.save_keys_called = false;

    // The batch contains b1 (index 1, already in chain at local_height=2) and b2 (index 2, new)
    svc.submitSyncBatch(batch, 2); // local_height=2 means blocks with index < 2 are overlap

    REQUIRE(bc.save_chunk_called);
    REQUIRE(bc.save_keys_called);
}

TEST_CASE("ChainService submitSyncBatch throws ValidationError on fork detection", "[chain_service]") {
    MockBlockchain bc;
    ChainService svc(bc);

    // Add a legit block to chain
    Block b1 = bc.createValidNextBlock("legit");
    bc.appendBlock(b1);

    // Create a fake block with different hash at same index
    Block fork_block;
    fork_block.index = 1;
    fork_block.hash = "fake_hash_not_matching";
    fork_block.prevHash = bc.blocks[0].hash;

    std::vector<Block> batch = {fork_block};
    REQUIRE_THROWS_AS(svc.submitSyncBatch(batch, 2), ValidationError);
}

TEST_CASE("ChainService read-through methods delegate to IChainReader", "[chain_service]") {
    MockBlockchain bc;
    ChainService svc(bc);

    REQUIRE(svc.getChainHeight() == bc.getChainBlockCount());
    REQUIRE(svc.getBlockAtTip().hash == bc.blocks.back().hash);
    REQUIRE(svc.getConsensusConfig().initialDifficulty == bc.getConfig().initialDifficulty);
}
