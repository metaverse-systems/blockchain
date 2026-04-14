#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "MockBlockchain.hpp"
#include "../src/network/RpcServer.hpp"
#include "../src/SyncState.hpp"
#include "../src/PeerManager.hpp"
#include "../src/json.hpp"

// RpcHandlerTests: friend class that accesses private handler methods on RpcServer.
// Creates a real RpcServer with a test io_context/ssl_context and MockBlockchain,
// then calls handler methods directly to verify actual production handler logic.
class RpcHandlerTests {
public:
    boost::asio::io_context io;
    boost::asio::ssl::context ssl_ctx;
    MockBlockchain bc;
    std::shared_ptr<RpcServer> server;

    RpcHandlerTests()
        : ssl_ctx(boost::asio::ssl::context::tls_server)
    {
        server = RpcServer::create(io, ssl_ctx, bc);
    }

    // Build a minimal JSON-RPC request
    static nlohmann::json makeRequest(int id, const std::string &method,
                                       const nlohmann::json &params = nullptr) {
        nlohmann::json req;
        req["jsonrpc"] = "2.0";
        req["id"] = id;
        req["method"] = method;
        req["params"] = params;
        return req;
    }

    nlohmann::json callGetNodeStatus(int id = 1) {
        return server->handle_getNodeStatus(makeRequest(id, "getNodeStatus"));
    }

    nlohmann::json callGetBlockRange(int id, nlohmann::json params) {
        return server->handle_getBlockRange(makeRequest(id, "getBlockRange", params));
    }

    nlohmann::json callGetChainLength(int id = 1) {
        return server->handle_getChainLength(makeRequest(id, "getChainLength"));
    }

    nlohmann::json callGetChunkCount(int id = 1) {
        return server->handle_getChunkCount(makeRequest(id, "getChunkCount"));
    }

    void setSyncStatus(SyncStatus *status) { server->set_sync_status(status); }
    void setPeerManager(PeerManager *pm) { server->set_peer_manager(pm); }
};

// =========================================================================
// getNodeStatus tests
// =========================================================================

TEST_CASE("getNodeStatus: response contains all 7 fields", "[rpc][getNodeStatus]") {
    RpcHandlerTests h;
    h.bc.publish("s", "k", "d", {"k"});

    auto response = h.callGetNodeStatus();

    REQUIRE(response.contains("result"));
    auto result = response["result"];
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
    REQUIRE(result["syncState"].get<std::string>() == "idle");
    REQUIRE(response["id"].get<int>() == 1);
}

TEST_CASE("getNodeStatus: sync active shows syncing", "[rpc][getNodeStatus]") {
    RpcHandlerTests h;
    SyncStatus sync;
    sync.isSyncing.store(true);
    h.setSyncStatus(&sync);

    auto response = h.callGetNodeStatus();
    auto result = response["result"];

    REQUIRE(result["syncState"].get<std::string>() == "syncing");
}

TEST_CASE("getNodeStatus: no peer_manager shows zero peer counts", "[rpc][getNodeStatus]") {
    RpcHandlerTests h;

    auto response = h.callGetNodeStatus();
    auto result = response["result"];

    REQUIRE(result["inboundPeers"].get<size_t>() == 0);
    REQUIRE(result["outboundPeers"].get<size_t>() == 0);
    REQUIRE(result["nodeUuid"].get<std::string>().empty());
}

// =========================================================================
// getBlockRange tests
// =========================================================================

TEST_CASE("getBlockRange: valid range returns correct blocks in order", "[rpc][getBlockRange]") {
    RpcHandlerTests h;
    h.bc.publish("s", "k1", "d1", {"k1"});
    h.bc.publish("s", "k2", "d2", {"k2"});
    h.bc.publish("s", "k3", "d3", {"k3"});

    nlohmann::json params;
    params["startIndex"] = 0;
    params["endIndex"] = 2;
    auto response = h.callGetBlockRange(1, params);

    REQUIRE(response.contains("result"));
    auto blocks = nlohmann::json::parse(response["result"].get<std::string>());
    REQUIRE(blocks.size() == 3);
    REQUIRE(blocks[0]["index"].get<size_t>() == 0);
    REQUIRE(blocks[1]["index"].get<size_t>() == 1);
    REQUIRE(blocks[2]["index"].get<size_t>() == 2);
}

TEST_CASE("getBlockRange: end index clamped when beyond chain length", "[rpc][getBlockRange]") {
    RpcHandlerTests h;
    h.bc.publish("s", "k1", "d1", {"k1"});

    nlohmann::json params;
    params["startIndex"] = 0;
    params["endIndex"] = 100;
    auto response = h.callGetBlockRange(1, params);

    REQUIRE(response.contains("result"));
    auto blocks = nlohmann::json::parse(response["result"].get<std::string>());
    REQUIRE(blocks.size() == 2);
    REQUIRE(blocks[0]["index"].get<size_t>() == 0);
    REQUIRE(blocks[1]["index"].get<size_t>() == 1);
}

TEST_CASE("getBlockRange: headersOnly=true returns header-only objects", "[rpc][getBlockRange]") {
    RpcHandlerTests h;
    h.bc.publish("s", "k1", "d1", {"k1"});

    nlohmann::json params;
    params["startIndex"] = 0;
    params["endIndex"] = 1;
    params["headersOnly"] = true;
    auto response = h.callGetBlockRange(1, params);

    REQUIRE(response.contains("result"));
    auto blocks = nlohmann::json::parse(response["result"].get<std::string>());
    REQUIRE(blocks.size() == 2);
    REQUIRE_FALSE(blocks[0].contains("entries"));
    REQUIRE_FALSE(blocks[1].contains("entries"));
    REQUIRE(blocks[0].contains("index"));
    REQUIRE(blocks[0].contains("hash"));
    REQUIRE(blocks[0].contains("merkleRoot"));
}

TEST_CASE("getBlockRange: start > end returns error -32602", "[rpc][getBlockRange]") {
    RpcHandlerTests h;

    nlohmann::json params;
    params["startIndex"] = 10;
    params["endIndex"] = 5;
    auto response = h.callGetBlockRange(1, params);

    REQUIRE(response.contains("error"));
    REQUIRE(response["error"]["code"].get<int>() == -32602);
    REQUIRE(response["error"]["message"].get<std::string>().find("startIndex exceeds endIndex") != std::string::npos);
}

TEST_CASE("getBlockRange: start beyond chain returns error -32001", "[rpc][getBlockRange]") {
    RpcHandlerTests h;

    nlohmann::json params;
    params["startIndex"] = 5;
    params["endIndex"] = 10;
    auto response = h.callGetBlockRange(1, params);

    REQUIRE(response.contains("error"));
    REQUIRE(response["error"]["code"].get<int>() == -32001);
    REQUIRE(response["error"]["message"].get<std::string>().find("Start index out of range") != std::string::npos);
}

TEST_CASE("getBlockRange: range exceeding 1000 returns error -32602", "[rpc][getBlockRange]") {
    RpcHandlerTests h;

    nlohmann::json params;
    params["startIndex"] = 0;
    params["endIndex"] = 1500;
    auto response = h.callGetBlockRange(1, params);

    REQUIRE(response.contains("error"));
    REQUIRE(response["error"]["code"].get<int>() == -32602);
    REQUIRE(response["error"]["message"].get<std::string>().find("Range too large") != std::string::npos);
}

TEST_CASE("getBlockRange: missing params returns error -32602", "[rpc][getBlockRange]") {
    RpcHandlerTests h;

    auto response = h.callGetBlockRange(1, nlohmann::json::object());

    REQUIRE(response.contains("error"));
    REQUIRE(response["error"]["code"].get<int>() == -32602);
}

TEST_CASE("getBlockRange: start=0 end=0 returns genesis block only", "[rpc][getBlockRange]") {
    RpcHandlerTests h;

    nlohmann::json params;
    params["startIndex"] = 0;
    params["endIndex"] = 0;
    auto response = h.callGetBlockRange(1, params);

    REQUIRE(response.contains("result"));
    auto blocks = nlohmann::json::parse(response["result"].get<std::string>());
    REQUIRE(blocks.size() == 1);
    REQUIRE(blocks[0]["index"].get<size_t>() == 0);
}

// =========================================================================
// getChainLength tests
// =========================================================================

TEST_CASE("getChainLength: returns correct integer for chain with multiple blocks", "[rpc][getChainLength]") {
    RpcHandlerTests h;
    h.bc.publish("s", "k1", "d1", {"k1"});
    h.bc.publish("s", "k2", "d2", {"k2"});
    h.bc.publish("s", "k3", "d3", {"k3"});

    auto response = h.callGetChainLength();

    REQUIRE(response.contains("result"));
    REQUIRE(response["result"].get<std::string>() == "4");
}

TEST_CASE("getChainLength: returns 1 for genesis-only chain", "[rpc][getChainLength]") {
    RpcHandlerTests h;

    auto response = h.callGetChainLength();

    REQUIRE(response.contains("result"));
    REQUIRE(response["result"].get<std::string>() == "1");
}

// =========================================================================
// getChunkCount tests
// =========================================================================

TEST_CASE("getChunkCount: returns correct chunk count", "[rpc][getChunkCount]") {
    RpcHandlerTests h;
    h.bc.publish("s", "k1", "d1", {"k1"});

    auto response = h.callGetChunkCount();

    REQUIRE(response.contains("result"));
    REQUIRE(response["result"].get<std::string>() == "1");
}

TEST_CASE("getChunkCount: returns 1 for genesis-only chain", "[rpc][getChunkCount]") {
    RpcHandlerTests h;

    auto response = h.callGetChunkCount();

    REQUIRE(response.contains("result"));
    REQUIRE(response["result"].get<std::string>() == "1");
}
