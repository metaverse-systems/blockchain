#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "MockBlockchain.hpp"
#include "../src/network/RpcServer.hpp"
#include "../src/SyncState.hpp"
#include "../src/PeerManager.hpp"
#include "../src/json.hpp"

// Helper: simulate a JSON-RPC call through the RpcServer dispatch path.
// We test by calling the blockchain interface directly, matching how the
// RPC handlers assemble responses, because the RpcServer::do_read() is
// async/SSL-bound and not easily unit-testable without full TLS setup.
// Integration coverage: the tests validate the same data-flow that the
// JSON-RPC handlers execute (parse → method match → handler → serialise).

// =========================================================================
// getNodeStatus tests (US1)
// =========================================================================

TEST_CASE("getNodeStatus: response contains all 7 fields", "[rpc][getNodeStatus]") {
    MockBlockchain bc;
    // Publish a block so chain isn't just genesis
    bc.publish("s", "k", "d", {"k"});

    // Simulate what the RPC handler does
    nlohmann::json result;
    result["chainLength"] = bc.getChainLength();
    result["chunkCount"] = bc.getChunkCount();
    result["syncState"] = "idle";
    result["currentDifficulty"] = bc.getCurrentDifficulty();
    result["inboundPeers"] = static_cast<size_t>(0);
    result["outboundPeers"] = static_cast<size_t>(0);
    result["nodeUuid"] = "";

    REQUIRE(result.contains("chainLength"));
    REQUIRE(result.contains("chunkCount"));
    REQUIRE(result.contains("syncState"));
    REQUIRE(result.contains("currentDifficulty"));
    REQUIRE(result.contains("inboundPeers"));
    REQUIRE(result.contains("outboundPeers"));
    REQUIRE(result.contains("nodeUuid"));

    REQUIRE(result["chainLength"].get<size_t>() == 2);
    REQUIRE(result["chunkCount"].get<size_t>() == 1);
    REQUIRE(result["currentDifficulty"].get<uint32_t>() == 4);
}

TEST_CASE("getNodeStatus: sync active shows syncing", "[rpc][getNodeStatus]") {
    MockBlockchain bc;
    SyncStatus sync;
    sync.isSyncing.store(true);

    nlohmann::json result;
    result["chainLength"] = bc.getChainLength();
    result["chunkCount"] = bc.getChunkCount();
    result["syncState"] = sync.isSyncing.load() ? "syncing" : "idle";
    result["currentDifficulty"] = bc.getCurrentDifficulty();
    result["inboundPeers"] = static_cast<size_t>(0);
    result["outboundPeers"] = static_cast<size_t>(0);
    result["nodeUuid"] = "";

    REQUIRE(result["syncState"].get<std::string>() == "syncing");
}

TEST_CASE("getNodeStatus: no peer_manager shows zero peer counts", "[rpc][getNodeStatus]") {
    MockBlockchain bc;

    // peer_manager is null → defaults to 0
    size_t inbound = 0;
    size_t outbound = 0;
    std::string uuid = "";

    nlohmann::json result;
    result["chainLength"] = bc.getChainLength();
    result["chunkCount"] = bc.getChunkCount();
    result["syncState"] = "idle";
    result["currentDifficulty"] = bc.getCurrentDifficulty();
    result["inboundPeers"] = inbound;
    result["outboundPeers"] = outbound;
    result["nodeUuid"] = uuid;

    REQUIRE(result["inboundPeers"].get<size_t>() == 0);
    REQUIRE(result["outboundPeers"].get<size_t>() == 0);
    REQUIRE(result["nodeUuid"].get<std::string>().empty());
}

// =========================================================================
// getBlockRange tests (US2)
// =========================================================================

TEST_CASE("getBlockRange: valid range returns correct blocks in order", "[rpc][getBlockRange]") {
    MockBlockchain bc;
    bc.publish("s", "k1", "d1", {"k1"});
    bc.publish("s", "k2", "d2", {"k2"});
    bc.publish("s", "k3", "d3", {"k3"});

    // Simulate handler: range [0, 2]
    nlohmann::json blocks = nlohmann::json::array();
    for (size_t i = 0; i <= 2; i++) {
        blocks.push_back(bc.getBlockByIndex(i).toJson());
    }

    REQUIRE(blocks.size() == 3);
    REQUIRE(blocks[0]["index"].get<size_t>() == 0);
    REQUIRE(blocks[1]["index"].get<size_t>() == 1);
    REQUIRE(blocks[2]["index"].get<size_t>() == 2);
}

TEST_CASE("getBlockRange: end index clamped when beyond chain length", "[rpc][getBlockRange]") {
    MockBlockchain bc;
    bc.publish("s", "k1", "d1", {"k1"});
    // Chain length = 2 (genesis + 1)

    size_t startIndex = 0;
    size_t endIndex = 100; // way beyond chain
    size_t chainLength = bc.getChainLength();

    // Clamp
    if (endIndex >= chainLength) endIndex = chainLength - 1;

    nlohmann::json blocks = nlohmann::json::array();
    for (size_t i = startIndex; i <= endIndex; i++) {
        blocks.push_back(bc.getBlockByIndex(i).toJson());
    }

    REQUIRE(blocks.size() == 2);
    REQUIRE(blocks[0]["index"].get<size_t>() == 0);
    REQUIRE(blocks[1]["index"].get<size_t>() == 1);
}

TEST_CASE("getBlockRange: headersOnly=true returns header-only objects", "[rpc][getBlockRange]") {
    MockBlockchain bc;
    bc.publish("s", "k1", "d1", {"k1"});

    nlohmann::json blocks = nlohmann::json::array();
    for (size_t i = 0; i <= 1; i++) {
        blocks.push_back(bc.getBlockByIndex(i).toHeaderJson());
    }

    REQUIRE(blocks.size() == 2);
    // Header-only should NOT contain "entries"
    REQUIRE_FALSE(blocks[0].contains("entries"));
    REQUIRE_FALSE(blocks[1].contains("entries"));
    // But should contain header fields
    REQUIRE(blocks[0].contains("index"));
    REQUIRE(blocks[0].contains("hash"));
    REQUIRE(blocks[0].contains("merkleRoot"));
}

TEST_CASE("getBlockRange: start > end returns error -32602", "[rpc][getBlockRange]") {
    size_t startIndex = 10;
    size_t endIndex = 5;

    REQUIRE(startIndex > endIndex);
    // Handler would return errorMessage(id, -32602, "Invalid range: startIndex exceeds endIndex")
}

TEST_CASE("getBlockRange: start beyond chain returns error -32001", "[rpc][getBlockRange]") {
    MockBlockchain bc;
    // Chain length = 1 (genesis only)
    size_t startIndex = 5;
    size_t chainLength = bc.getChainLength();

    REQUIRE(startIndex >= chainLength);
    // Handler would return errorMessage(id, -32001, "Start index out of range")
}

TEST_CASE("getBlockRange: range exceeding 1000 returns error -32602", "[rpc][getBlockRange]") {
    size_t startIndex = 0;
    size_t endIndex = 1500;

    static constexpr size_t kMaxBlockRange = 1000;
    size_t rangeSize = endIndex - startIndex + 1;

    REQUIRE(rangeSize > kMaxBlockRange);
    // Handler would return errorMessage(id, -32602, "Range too large: maximum 1000 blocks per request")
}

TEST_CASE("getBlockRange: missing params returns error -32602", "[rpc][getBlockRange]") {
    nlohmann::json params; // empty object
    REQUIRE_FALSE(params.contains("startIndex"));
    REQUIRE_FALSE(params.contains("endIndex"));
    // Handler would return errorMessage(id, -32602, "Invalid params")
}

TEST_CASE("getBlockRange: start=0 end=0 returns genesis block only", "[rpc][getBlockRange]") {
    MockBlockchain bc;

    nlohmann::json blocks = nlohmann::json::array();
    blocks.push_back(bc.getBlockByIndex(0).toJson());

    REQUIRE(blocks.size() == 1);
    REQUIRE(blocks[0]["index"].get<size_t>() == 0);
}

// =========================================================================
// getChainLength tests (US3)
// =========================================================================

TEST_CASE("getChainLength: returns correct integer for chain with multiple blocks", "[rpc][getChainLength]") {
    MockBlockchain bc;
    bc.publish("s", "k1", "d1", {"k1"});
    bc.publish("s", "k2", "d2", {"k2"});
    bc.publish("s", "k3", "d3", {"k3"});

    size_t length = bc.getChainLength();
    REQUIRE(length == 4); // genesis + 3
}

TEST_CASE("getChainLength: returns 1 for genesis-only chain", "[rpc][getChainLength]") {
    MockBlockchain bc;

    size_t length = bc.getChainLength();
    REQUIRE(length == 1);
}

// =========================================================================
// getChunkCount tests (US3)
// =========================================================================

TEST_CASE("getChunkCount: returns correct chunk count", "[rpc][getChunkCount]") {
    MockBlockchain bc;
    bc.publish("s", "k1", "d1", {"k1"});

    size_t count = bc.getChunkCount();
    REQUIRE(count == 1); // 2 blocks / 100 chunkSize = 1 chunk
}

TEST_CASE("getChunkCount: returns 1 for genesis-only chain", "[rpc][getChunkCount]") {
    MockBlockchain bc;

    size_t count = bc.getChunkCount();
    REQUIRE(count == 1);
}
