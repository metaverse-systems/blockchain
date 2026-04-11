#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/Blockchain.hpp"
#include "../src/MockChunk.hpp"
#include "../src/Chunk.hpp"
#include "../src/MerkleTree.hpp"
#include <filesystem>
#include <sstream>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

TEST_CASE("Block Construction with entries", "[Block]")
{
    StreamEntry e;
    e.stream = "test-stream";
    e.key = "test-key";
    e.data = "test data";

    Block b(0, 0, "0", {e});

    REQUIRE(b.index == 0);
    REQUIRE(b.timestamp == 0);
    REQUIRE(b.prevHash == "0");
    REQUIRE(b.entries.size() == 1);
    REQUIRE(b.entries[0].stream == "test-stream");
    REQUIRE(b.entries[0].key == "test-key");
    REQUIRE(b.entries[0].data == "test data");
    REQUIRE(!b.hash.empty());
}

TEST_CASE("Block serialization round-trip preserves entries", "[Block]")
{
    StreamEntry e1;
    e1.stream = "assets";
    e1.key = "item-1";
    e1.data = "sword";

    StreamEntry e2;
    e2.stream = "logs";
    e2.key = "event-1";
    e2.data = "login";

    Block original(1, 12345, "prevhash", {e1, e2}, 42, 1);

    std::ostringstream oss;
    {
        boost::archive::binary_oarchive oa(oss);
        oa << original;
    }

    Block restored;
    std::istringstream iss(oss.str());
    {
        boost::archive::binary_iarchive ia(iss);
        ia >> restored;
    }

    REQUIRE(restored.index == original.index);
    REQUIRE(restored.timestamp == original.timestamp);
    REQUIRE(restored.prevHash == original.prevHash);
    REQUIRE(restored.hash == original.hash);
    REQUIRE(restored.nonce == original.nonce);
    REQUIRE(restored.difficulty == original.difficulty);
    REQUIRE(restored.entries.size() == 2);
    REQUIRE(restored.entries[0].stream == "assets");
    REQUIRE(restored.entries[0].key == "item-1");
    REQUIRE(restored.entries[1].stream == "logs");
    REQUIRE(restored.entries[1].key == "event-1");
}

TEST_CASE("Block calculateHash changes when entries differ", "[Block]")
{
    StreamEntry e1;
    e1.stream = "s";
    e1.key = "k";
    e1.data = "data1";

    StreamEntry e2;
    e2.stream = "s";
    e2.key = "k";
    e2.data = "data2";

    Block b1(1, 100, "prev", {e1}, 0, 0);
    Block b2(1, 100, "prev", {e2}, 0, 0);

    REQUIRE(b1.hash != b2.hash);
}

TEST_CASE("Chunk::save() throws catchable std::runtime_error", "[Chunk]")
{
    Chunk c(0, "/nonexistent_dir_that_does_not_exist_xyz");
    REQUIRE_THROWS_AS(c.save(), std::runtime_error);
}

TEST_CASE("publish data integrity and chunk boundary", "[Blockchain]")
{
    Blockchain<MockChunk> bc(".");

    SECTION("Publish 5 entries and retrieve all by index")
    {
        std::vector<Block> added;
        for (int i = 0; i < 5; i++) {
            auto b = bc.publish("stream-" + std::to_string(i), "key-" + std::to_string(i),
                                "data_" + std::to_string(i), {"indexkey_" + std::to_string(i)});
            added.push_back(b);
        }

        for (size_t i = 0; i < added.size(); i++) {
            Block retrieved = bc.getBlockByIndex(added[i].index);
            REQUIRE(retrieved.index == added[i].index);
            REQUIRE(retrieved.hash == added[i].hash);
            REQUIRE(retrieved.entries.size() == 1);
            REQUIRE(retrieved.entries[0].data == "data_" + std::to_string(i));
        }
    }

    SECTION("Publish 100 blocks to trigger new chunk boundary")
    {
        for (int i = 0; i < 100; i++) {
            bc.publish("stream", "key", "block_" + std::to_string(i), {"key"});
        }

        Block last = bc.getBlockByIndex(100);
        REQUIRE(last.entries[0].data == "block_99");

        Block first = bc.getBlockByIndex(1);
        REQUIRE(first.entries[0].data == "block_0");
    }
}

TEST_CASE("getBlocksByKeys groups loads by chunk", "[Blockchain]")
{
    Blockchain<MockChunk> bc(".");

    bc.publish("s", "k", "a", {"alpha"});
    bc.publish("s", "k", "b", {"beta"});
    bc.publish("s", "k", "c", {"alpha"});

    auto blocks = bc.getBlocksByKeys({"alpha", "beta"});

    REQUIRE(blocks.size() == 3);
}

TEST_CASE("Block toJson includes entries array", "[Block]")
{
    StreamEntry e;
    e.stream = "assets";
    e.key = "item-42";
    e.data = "test";

    Block b(1, 100, "prev", {e}, 0, 0);
    auto j = b.toJson();

    REQUIRE(j.contains("entries"));
    REQUIRE(j["entries"].is_array());
    REQUIRE(j["entries"].size() == 1);
    REQUIRE(j["entries"][0]["stream"] == "assets");
    REQUIRE(j["entries"][0]["key"] == "item-42");
    REQUIRE(j["entries"][0]["data"] == "test");
    REQUIRE_FALSE(j.contains("data"));
}

// ============================================================
// T014: merkleRoot in Block
// ============================================================

TEST_CASE("Block merkleRoot is populated on construction", "[Block][merkle]") {
    StreamEntry e;
    e.stream = "test-stream";
    e.key = "test-key";
    e.data = "test data";

    Block b(1, 100, "prev", {e}, 0, 0);

    REQUIRE_FALSE(b.merkleRoot.empty());
    REQUIRE(b.merkleRoot.size() == 64); // SHA-256 hex
}

TEST_CASE("Block hash incorporates merkleRoot", "[Block][merkle]") {
    StreamEntry e1;
    e1.stream = "s";
    e1.key = "k";
    e1.data = "data1";

    StreamEntry e2;
    e2.stream = "s";
    e2.key = "k";
    e2.data = "data2";

    Block b1(1, 100, "prev", {e1}, 0, 0);
    Block b2(1, 100, "prev", {e2}, 0, 0);

    // Different entries → different merkleRoots → different hashes
    REQUIRE(b1.merkleRoot != b2.merkleRoot);
    REQUIRE(b1.hash != b2.hash);
}

TEST_CASE("Block serialization round-trip preserves merkleRoot", "[Block][merkle]") {
    StreamEntry e;
    e.stream = "assets";
    e.key = "item-1";
    e.data = "sword";

    Block original(1, 12345, "prevhash", {e}, 42, 1);

    std::ostringstream oss;
    {
        boost::archive::binary_oarchive oa(oss);
        oa << original;
    }

    Block restored;
    std::istringstream iss(oss.str());
    {
        boost::archive::binary_iarchive ia(iss);
        ia >> restored;
    }

    REQUIRE(restored.merkleRoot == original.merkleRoot);
    REQUIRE(restored.merkleRoot.size() == 64);
    REQUIRE(restored.hash == original.hash);
}

TEST_CASE("Block toJson includes merkleRoot", "[Block][merkle]") {
    StreamEntry e;
    e.stream = "s";
    e.key = "k";
    e.data = "d";

    Block b(1, 100, "prev", {e}, 0, 0);
    auto j = b.toJson();

    REQUIRE(j.contains("merkleRoot"));
    REQUIRE(j["merkleRoot"].is_string());
    REQUIRE(j["merkleRoot"].get<std::string>() == b.merkleRoot);
}

TEST_CASE("Empty block has well-defined merkleRoot", "[Block][merkle]") {
    Block b(0, 0, "", {}, 0, 0);
    // SHA-256 of empty string
    REQUIRE(b.merkleRoot == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

// ============================================================
// T023: Block header tests (US3)
// ============================================================

TEST_CASE("toHeaderJson returns exactly 7 fields", "[Block][header][US3]") {
    StreamEntry e;
    e.stream = "test";
    e.key = "k";
    e.data = "data";

    Block b(1, 100, "prev", {e}, 42, 2);
    auto hj = b.toHeaderJson();

    REQUIRE(hj.size() == 7);
    REQUIRE(hj.contains("index"));
    REQUIRE(hj.contains("timestamp"));
    REQUIRE(hj.contains("prevHash"));
    REQUIRE(hj.contains("merkleRoot"));
    REQUIRE(hj.contains("nonce"));
    REQUIRE(hj.contains("difficulty"));
    REQUIRE(hj.contains("hash"));
    REQUIRE_FALSE(hj.contains("entries"));
}

TEST_CASE("toHeaderJson hash matches full block hash", "[Block][header][US3]") {
    StreamEntry e;
    e.stream = "assets";
    e.key = "item";
    e.data = "sword";

    Block b(1, 100, "prev", {e}, 0, 0);
    auto hj = b.toHeaderJson();

    REQUIRE(hj["hash"].get<std::string>() == b.hash);
    REQUIRE(hj["merkleRoot"].get<std::string>() == b.merkleRoot);
}