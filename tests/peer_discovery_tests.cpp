#include <catch2/catch_all.hpp>
#include "../src/PeerManager.hpp"
#include "../src/ChainService.hpp"
#include "../src/PeerConfig.hpp"
#include "../src/Blockchain.hpp"
#include "../src/MockChunk.hpp"
#include "../src/SyncState.hpp"
#include "../src/network/PeerMessages.hpp"
#include <filesystem>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

static std::filesystem::path make_temp_dir() {
    auto p = std::filesystem::temp_directory_path() / ("peer_disc_test_" + std::to_string(std::rand()));
    std::filesystem::create_directories(p);
    return p;
}

TEST_CASE("PeerExchangeRequest Boost.Serialization round-trip", "[PeerExchange]") {
    PeerExchangeRequest original;
    original.sender_uuid = "test-uuid-1234";
    original.sender_listen_port = 12346;
    original.peers.push_back({"10.0.0.1", 12346});
    original.peers.push_back({"10.0.0.2", 12346});

    std::stringstream ss;
    {
        boost::archive::binary_oarchive oa(ss);
        oa << original;
    }
    PeerExchangeRequest loaded;
    {
        boost::archive::binary_iarchive ia(ss);
        ia >> loaded;
    }

    REQUIRE(loaded.sender_uuid == original.sender_uuid);
    REQUIRE(loaded.sender_listen_port == original.sender_listen_port);
    REQUIRE(loaded.peers.size() == 2);
    REQUIRE(loaded.peers[0].host == "10.0.0.1");
    REQUIRE(loaded.peers[1].host == "10.0.0.2");
}

TEST_CASE("PeerExchangeResponse Boost.Serialization round-trip", "[PeerExchange]") {
    PeerExchangeResponse original;
    original.sender_uuid = "resp-uuid-5678";
    original.sender_listen_port = 22346;
    original.peers.push_back({"192.168.1.1", 33000});

    std::stringstream ss;
    {
        boost::archive::binary_oarchive oa(ss);
        oa << original;
    }
    PeerExchangeResponse loaded;
    {
        boost::archive::binary_iarchive ia(ss);
        ia >> loaded;
    }

    REQUIRE(loaded.sender_uuid == original.sender_uuid);
    REQUIRE(loaded.sender_listen_port == original.sender_listen_port);
    REQUIRE(loaded.peers.size() == 1);
    REQUIRE(loaded.peers[0].port == 33000);
}

TEST_CASE("PeerManager merges received peer list", "[PeerExchange]") {
    auto dir = make_temp_dir();
    boost::asio::io_context io;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv12);
    Blockchain<MockChunk> bc(".");
    SyncStatus sync;
    ChainService cs(bc);
    PeerConfig cfg;
    cfg.discovery_enabled = false; // Disable auto-connect for testing

    PeerManager pm(io, ssl_ctx, cfg, dir, bc, cs, sync);
    pm.load_peers();

    // Simulate receiving a peer exchange
    std::vector<PeerAddress> received = {
        {"10.0.0.1", 12346},
        {"10.0.0.2", 12346},
        {"10.0.0.3", 12346}
    };

    pm.on_peer_exchange_received("remote-uuid", 12346, "192.168.1.1", received);

    // Should have 4 peers: the sender + 3 received
    auto peers = pm.get_peers();
    REQUIRE(peers.size() == 4);

    // Sender should be recorded
    auto *sender = pm.find_peer("192.168.1.1", 12346);
    REQUIRE(sender != nullptr);
    REQUIRE(sender->node_uuid == "remote-uuid");

    std::filesystem::remove_all(dir);
}

TEST_CASE("PeerManager filters banned peers from exchange", "[PeerExchange]") {
    auto dir = make_temp_dir();
    boost::asio::io_context io;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv12);
    Blockchain<MockChunk> bc(".");
    SyncStatus sync;
    ChainService cs(bc);
    PeerConfig cfg;
    cfg.discovery_enabled = false;

    PeerManager pm(io, ssl_ctx, cfg, dir, bc, cs, sync);
    pm.load_peers();

    // Ban a peer first
    pm.ban_peer("10.0.0.1", 12346, "test", 3600);

    // Receive exchange that includes the banned peer
    std::vector<PeerAddress> received = {
        {"10.0.0.1", 12346},
        {"10.0.0.2", 12346}
    };

    pm.on_peer_exchange_received("remote-uuid", 12346, "192.168.1.1", received);

    // Banned peer should not be in the list
    REQUIRE(pm.find_peer("10.0.0.1", 12346) == nullptr);
    // Non-banned peer should be added
    REQUIRE(pm.find_peer("10.0.0.2", 12346) != nullptr);

    std::filesystem::remove_all(dir);
}

TEST_CASE("PeerManager get_non_banned_peer_addresses excludes banned", "[PeerExchange]") {
    auto dir = make_temp_dir();
    boost::asio::io_context io;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv12);
    Blockchain<MockChunk> bc(".");
    SyncStatus sync;
    ChainService cs(bc);
    PeerConfig cfg;

    PeerManager pm(io, ssl_ctx, cfg, dir, bc, cs, sync);
    pm.load_peers();

    PeerEntry e1{.host = "10.0.0.1", .port = 12346};
    PeerEntry e2{.host = "10.0.0.2", .port = 12346};
    pm.add_peer(e1);
    pm.add_peer(e2);

    pm.ban_peer("10.0.0.1", 12346, "test", 3600);

    auto addrs = pm.get_non_banned_peer_addresses();
    REQUIRE(addrs.size() == 1);
    REQUIRE(addrs[0].host == "10.0.0.2");

    std::filesystem::remove_all(dir);
}

TEST_CASE("PeerManager expired ban purge", "[PeerExchange]") {
    auto dir = make_temp_dir();
    boost::asio::io_context io;
    boost::asio::ssl::context ssl_ctx(boost::asio::ssl::context::tlsv12);
    Blockchain<MockChunk> bc(".");
    SyncStatus sync;
    ChainService cs(bc);
    PeerConfig cfg;

    PeerManager pm(io, ssl_ctx, cfg, dir, bc, cs, sync);
    pm.load_peers();

    // Create an already-expired ban (expires in the past)
    pm.ban_peer("10.0.0.1", 12346, "test", 0); // permanent ban
    REQUIRE(pm.is_banned("10.0.0.1", 12346));

    // Unban it
    pm.unban_peer("10.0.0.1", 12346);
    REQUIRE(!pm.is_banned("10.0.0.1", 12346));

    std::filesystem::remove_all(dir);
}
