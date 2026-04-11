#define CATCH_CONFIG_MAIN
#include <catch2/catch_all.hpp>
#include "IntegrationTestFixture.hpp"
#include <thread>
#include <chrono>

// Helper to get chain length via RPC
static size_t get_chain_length(RpcTestClient &client) {
    auto resp = client.call("getChainLength");
    auto result = resp["result"];
    if (result.is_number()) return result.get<size_t>();
    return std::stoul(result.get<std::string>());
}

// Helper to poll until chain length reaches target, with timeout
static bool wait_for_chain_length(RpcTestClient &client, size_t target, int timeout_seconds) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(timeout_seconds)) {
        try {
            if (get_chain_length(client) >= target) return true;
        } catch (...) {
            // Connection may not be ready yet
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
    return false;
}

// Helper to wait for peer connection to be established
static bool wait_for_outbound_peers(RpcTestClient &client, size_t count, int timeout_seconds) {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::seconds(timeout_seconds)) {
        try {
            auto resp = client.call("getNodeStatus");
            auto result = resp["result"];
            if (result.is_string()) {
                auto parsed = nlohmann::json::parse(result.get<std::string>());
                if (parsed["outboundPeers"].get<size_t>() >= count) return true;
            } else {
                if (result["outboundPeers"].get<size_t>() >= count) return true;
            }
        } catch (...) {}
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return false;
}

// =========================================================================
// T028: Block propagation test — publish on A after B connects, verify B receives it
// =========================================================================
TEST_CASE("P2P: New block on Node A propagates to connected Node B", "[p2p][integration]") {
    IntegrationTestFixture fixture;

    // Start both nodes without seed nodes (no auto-discovery)
    auto *nodeA = fixture.create_node({"teststream"});
    auto *nodeB = fixture.create_node({"teststream"});

    auto clientA = fixture.create_rpc_client(nodeA);
    auto clientB = fixture.create_rpc_client(nodeB);

    // Verify both start with genesis only
    REQUIRE(get_chain_length(*clientA) == 1);
    REQUIRE(get_chain_length(*clientB) == 1);

    // Manually connect Node B to Node A's P2P port
    nodeB->connect_to_peer("127.0.0.1", nodeA->p2p_port);

    // Wait for the connection to be established
    REQUIRE(wait_for_outbound_peers(*clientB, 1, 10));

    // Small delay for connection to fully stabilize
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Publish a block on Node A
    clientA->call("publish", {
        {"stream", "teststream"},
        {"key", "propkey"},
        {"data", "propdata"},
        {"keys", nlohmann::json::array({"propkey"})}
    });

    REQUIRE(get_chain_length(*clientA) == 2);

    // Wait for Node B to receive the propagated block (up to 10 seconds)
    REQUIRE(wait_for_chain_length(*clientB, 2, 10));
}

// =========================================================================
// T029: Multiple blocks propagate between connected nodes
// =========================================================================
TEST_CASE("P2P: Multiple blocks propagate from Node A to Node B", "[p2p][integration]") {
    IntegrationTestFixture fixture;

    auto *nodeA = fixture.create_node({"teststream"});
    auto *nodeB = fixture.create_node({"teststream"});

    auto clientA = fixture.create_rpc_client(nodeA);
    auto clientB = fixture.create_rpc_client(nodeB);

    // Connect Node B to Node A
    nodeB->connect_to_peer("127.0.0.1", nodeA->p2p_port);
    REQUIRE(wait_for_outbound_peers(*clientB, 1, 10));
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Publish 5 blocks on Node A, with delays so each propagates before the next
    for (int i = 0; i < 5; i++) {
        clientA->call("publish", {
            {"stream", "teststream"},
            {"key", "key" + std::to_string(i)},
            {"data", "data" + std::to_string(i)},
            {"keys", nlohmann::json::array({"key" + std::to_string(i)})}
        });
        // Wait for this block to propagate before publishing next
        REQUIRE(wait_for_chain_length(*clientB, 2 + i, 10));
    }

    REQUIRE(get_chain_length(*clientA) == 6); // genesis + 5
    REQUIRE(get_chain_length(*clientB) == 6);
}

// =========================================================================
// T030: Three-node propagation — block on A reaches C through B
// =========================================================================
TEST_CASE("P2P: Block propagates A -> B -> C via relay", "[p2p][integration]") {
    IntegrationTestFixture fixture;

    auto *nodeA = fixture.create_node({"teststream"});
    auto *nodeB = fixture.create_node({"teststream"});
    auto *nodeC = fixture.create_node({"teststream"});

    auto clientA = fixture.create_rpc_client(nodeA);
    auto clientB = fixture.create_rpc_client(nodeB);
    auto clientC = fixture.create_rpc_client(nodeC);

    // Connect B to A, and C to B (chain topology: A <- B <- C)
    nodeB->connect_to_peer("127.0.0.1", nodeA->p2p_port);
    REQUIRE(wait_for_outbound_peers(*clientB, 1, 10));

    nodeC->connect_to_peer("127.0.0.1", nodeB->p2p_port);
    REQUIRE(wait_for_outbound_peers(*clientC, 1, 10));

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    // Publish a block on Node A
    clientA->call("publish", {
        {"stream", "teststream"},
        {"key", "relaykey"},
        {"data", "relaydata"},
        {"keys", nlohmann::json::array({"relaykey"})}
    });

    REQUIRE(get_chain_length(*clientA) == 2);

    // Wait for block to propagate to B and then to C
    REQUIRE(wait_for_chain_length(*clientB, 2, 10));
    REQUIRE(wait_for_chain_length(*clientC, 2, 15));
}
