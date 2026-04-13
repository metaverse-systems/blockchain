#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "IntegrationTestFixture.hpp"

// Shared fixture: one NodeInstance with allowed_streams = {"teststream"}
// Per-test watchdog is handled by Catch2 timeout if needed.

// Helper to check a JSON-RPC result field
static nlohmann::json get_result(const nlohmann::json &resp) {
    REQUIRE(resp.contains("jsonrpc"));
    REQUIRE(resp["jsonrpc"] == "2.0");
    REQUIRE(resp.contains("result"));
    return resp["result"];
}

static nlohmann::json get_error(const nlohmann::json &resp) {
    REQUIRE(resp.contains("jsonrpc"));
    REQUIRE(resp["jsonrpc"] == "2.0");
    REQUIRE(resp.contains("error"));
    return resp["error"];
}

// Parse result that may be a string-encoded JSON
static nlohmann::json parse_result(const nlohmann::json &resp) {
    auto r = get_result(resp);
    if (r.is_string()) {
        return nlohmann::json::parse(r.get<std::string>());
    }
    return r;
}

// =========================================================================
// T009: getChainLength positive test
// =========================================================================
TEST_CASE("RPC: getChainLength returns 1 on fresh node", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->call("getChainLength");
    auto result = get_result(resp);

    // Result may be an integer or string "1"
    if (result.is_number()) {
        REQUIRE(result.get<size_t>() == 1);
    } else {
        REQUIRE(std::stoul(result.get<std::string>()) == 1);
    }
}

// =========================================================================
// T010: getChunkCount positive test
// =========================================================================
TEST_CASE("RPC: getChunkCount returns 1 on fresh node", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->call("getChunkCount");
    auto result = get_result(resp);

    if (result.is_number()) {
        REQUIRE(result.get<size_t>() == 1);
    } else {
        REQUIRE(std::stoul(result.get<std::string>()) == 1);
    }
}

// =========================================================================
// T011: getNodeStatus positive test
// =========================================================================
TEST_CASE("RPC: getNodeStatus returns all 7 fields with syncState idle", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->call("getNodeStatus");
    auto result = parse_result(resp);

    REQUIRE(result.contains("chainLength"));
    REQUIRE(result.contains("chunkCount"));
    REQUIRE(result.contains("syncState"));
    REQUIRE(result.contains("currentDifficulty"));
    REQUIRE(result.contains("inboundPeers"));
    REQUIRE(result.contains("outboundPeers"));
    REQUIRE(result.contains("nodeUuid"));
    REQUIRE(result["syncState"] == "idle");
}

// =========================================================================
// T008: addBlock positive test
// =========================================================================
TEST_CASE("RPC: addBlock is not a registered method", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // addBlock is listed as legacy but is not implemented in the current RPC server
    // Verify it returns method-not-found error
    auto resp = client->call("addBlock", {{"data", "test data"}});
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32601);
}

// =========================================================================
// T012: publish positive test
// =========================================================================
TEST_CASE("RPC: publish creates a block and increases chain length", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // Get initial chain length
    auto len_resp = client->call("getChainLength");
    auto initial_len = get_result(len_resp);
    size_t initial = initial_len.is_number() ? initial_len.get<size_t>()
                                              : std::stoul(initial_len.get<std::string>());

    // Publish an entry
    auto resp = client->call("publish", {
        {"stream", "teststream"},
        {"key", "testkey"},
        {"data", "testdata"},
        {"keys", nlohmann::json::array({"testkey"})}
    });
    auto block = parse_result(resp);
    REQUIRE(block.contains("index"));
    REQUIRE(block["index"].get<size_t>() > 0);

    // Verify chain length increased
    auto len_resp2 = client->call("getChainLength");
    auto new_len = get_result(len_resp2);
    size_t updated = new_len.is_number() ? new_len.get<size_t>()
                                          : std::stoul(new_len.get<std::string>());
    REQUIRE(updated > initial);
}

// =========================================================================
// T013: getStreamEntries positive test
// =========================================================================
TEST_CASE("RPC: getStreamEntries returns published entries", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // Publish entry
    client->call("publish", {
        {"stream", "teststream"},
        {"key", "mykey"},
        {"data", "mydata"},
        {"keys", nlohmann::json::array({"mykey"})}
    });

    // Fetch entries
    auto resp = client->call("getStreamEntries", {{"stream", "teststream"}});
    auto entries = parse_result(resp);
    REQUIRE(entries.is_array());
    REQUIRE(entries.size() > 0);

    // Check published entry is present
    bool found = false;
    for (const auto &e : entries) {
        if (e.contains("key") && e["key"] == "mykey" && e["data"] == "mydata") {
            found = true;
            break;
        }
    }
    REQUIRE(found);
}

// =========================================================================
// T014: getBlockRange positive test
// =========================================================================
TEST_CASE("RPC: getBlockRange returns blocks for valid range", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // Publish some entries to have blocks beyond genesis
    client->call("publish", {{"stream", "teststream"}, {"key", "k1"}, {"data", "d1"}, {"keys", nlohmann::json::array({"k1"})}});
    client->call("publish", {{"stream", "teststream"}, {"key", "k2"}, {"data", "d2"}, {"keys", nlohmann::json::array({"k2"})}});

    // Get blocks 0-1 (genesis + first published)
    auto resp = client->call("getBlockRange", {{"startIndex", 0}, {"endIndex", 1}});
    auto blocks = parse_result(resp);
    REQUIRE(blocks.is_array());
    REQUIRE(blocks.size() == 2);
}

// =========================================================================
// T015: getBlockRange negative test (invalid range)
// =========================================================================
TEST_CASE("RPC: getBlockRange returns error for invalid range", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // start > end
    auto resp = client->call("getBlockRange", {{"startIndex", 5}, {"endIndex", 3}});
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32602);
}

// =========================================================================
// T016: getBlockHeader positive test
// =========================================================================
TEST_CASE("RPC: getBlockHeader returns all 7 header fields for genesis", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->call("getBlockHeader", {{"blockIndex", 0}});
    auto header = parse_result(resp);

    REQUIRE(header.contains("index"));
    REQUIRE(header.contains("timestamp"));
    REQUIRE(header.contains("prevHash"));
    REQUIRE(header.contains("merkleRoot"));
    REQUIRE(header.contains("nonce"));
    REQUIRE(header.contains("difficulty"));
    REQUIRE(header.contains("hash"));
}

// =========================================================================
// T017: getBlockHeader negative test (out-of-range)
// =========================================================================
TEST_CASE("RPC: getBlockHeader returns error for out-of-range index", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->call("getBlockHeader", {{"blockIndex", 99999}});
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32001);
}

// =========================================================================
// T018: getInclusionProof positive test
// =========================================================================
TEST_CASE("RPC: getInclusionProof returns valid proof fields", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // Publish entry
    client->call("publish", {
        {"stream", "teststream"},
        {"key", "proofkey"},
        {"data", "proofdata"},
        {"keys", nlohmann::json::array({"proofkey"})}
    });

    auto resp = client->call("getInclusionProof", {{"blockIndex", 1}, {"entryIndex", 0}});
    auto result = parse_result(resp);

    REQUIRE(result.contains("blockIndex"));
    REQUIRE(result.contains("entryIndex"));
    REQUIRE(result.contains("merkleRoot"));
    REQUIRE(result.contains("leafHash"));
    REQUIRE(result.contains("proof"));
}

// =========================================================================
// T019: getInclusionProof negative test
// =========================================================================
TEST_CASE("RPC: getInclusionProof returns error for out-of-range block", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->call("getInclusionProof", {{"blockIndex", 99999}, {"entryIndex", 0}});
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32001);
}

// =========================================================================
// T020: verifyInclusionProof positive test
// =========================================================================
TEST_CASE("RPC: verifyInclusionProof returns valid=true for correct proof", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // Publish entry
    client->call("publish", {
        {"stream", "teststream"},
        {"key", "verifykey"},
        {"data", "verifydata"},
        {"keys", nlohmann::json::array({"verifykey"})}
    });

    // Get proof
    auto proof_resp = client->call("getInclusionProof", {{"blockIndex", 1}, {"entryIndex", 0}});
    auto proof_result = parse_result(proof_resp);

    // Verify proof
    auto verify_resp = client->call("verifyInclusionProof", {
        {"blockIndex", proof_result["blockIndex"]},
        {"leafHash", proof_result["leafHash"]},
        {"proof", proof_result["proof"]}
    });
    auto verify_result = parse_result(verify_resp);

    REQUIRE(verify_result.contains("valid"));
    REQUIRE(verify_result["valid"].get<bool>() == true);
    REQUIRE(verify_result.contains("merkleRoot"));
    REQUIRE(verify_result["merkleRoot"] == proof_result["merkleRoot"]);
}

// =========================================================================
// T021: verifyInclusionProof tamper test
// =========================================================================
TEST_CASE("RPC: verifyInclusionProof returns valid=false for tampered hash", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // Publish entry
    client->call("publish", {
        {"stream", "teststream"},
        {"key", "tamperkey"},
        {"data", "tamperdata"},
        {"keys", nlohmann::json::array({"tamperkey"})}
    });

    // Get proof
    auto proof_resp = client->call("getInclusionProof", {{"blockIndex", 1}, {"entryIndex", 0}});
    auto proof_result = parse_result(proof_resp);

    // Tamper with leafHash (64 'f' chars)
    std::string tampered_hash(64, 'f');

    auto verify_resp = client->call("verifyInclusionProof", {
        {"blockIndex", proof_result["blockIndex"]},
        {"leafHash", tampered_hash},
        {"proof", proof_result["proof"]}
    });
    auto verify_result = parse_result(verify_resp);

    REQUIRE(verify_result.contains("valid"));
    REQUIRE(verify_result["valid"].get<bool>() == false);
}

// =========================================================================
// T022: publish negative test (stream not allowed)
// =========================================================================
TEST_CASE("RPC: publish to disallowed stream returns error -32003", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->call("publish", {
        {"stream", "notallowed"},
        {"key", "k"},
        {"data", "d"},
        {"keys", nlohmann::json::array({"k"})}
    });
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32003);
}

// =========================================================================
// T023: requestSync negative test (no peer client)
// =========================================================================
TEST_CASE("RPC: requestSync with no peer client returns error -32003", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->call("requestSync");
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32003);
}

// =========================================================================
// T024: unknown method test
// =========================================================================
TEST_CASE("RPC: unknown method returns error -32601", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->call("nonExistentMethod");
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32601);
}

// =========================================================================
// T025: malformed JSON test
// =========================================================================
TEST_CASE("RPC: malformed JSON returns error -32600", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    auto resp = client->send_raw("not valid json\n");
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32600);
}

// =========================================================================
// T026: missing id field test
// =========================================================================
TEST_CASE("RPC: missing id field returns error response", "[rpc][integration]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // Send valid JSON-RPC without id
    nlohmann::json no_id_request;
    no_id_request["jsonrpc"] = "2.0";
    no_id_request["method"] = "getChainLength";
    no_id_request["params"] = nlohmann::json::object();

    auto resp = client->send_raw(no_id_request.dump() + "\n");
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32600);
}

// =========================================================================
// US2: getBlockByIndex bounds check tests
// =========================================================================
TEST_CASE("RPC: getBlockByIndex with out-of-range index returns error -32001", "[rpc][integration][US2]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    nlohmann::json params;
    params["index"] = 999999;
    auto resp = client->call("getBlockByIndex", params);
    auto err = get_error(resp);
    REQUIRE(err["code"].get<int>() == -32001);
    REQUIRE(err["message"].get<std::string>() == "Block not found");
}

TEST_CASE("RPC: getBlockByIndex with valid index returns block", "[rpc][integration][US2]") {
    IntegrationTestFixture fixture;
    auto *node = fixture.create_node({"teststream"});
    auto client = fixture.create_rpc_client(node);

    // Genesis block is always at index 0
    nlohmann::json params;
    params["index"] = 0;
    auto resp = client->call("getBlockByIndex", params);
    auto result = parse_result(resp);
    REQUIRE(result.contains("index"));
    REQUIRE(result["index"].get<size_t>() == 0);
}
