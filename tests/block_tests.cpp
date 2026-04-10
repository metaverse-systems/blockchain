#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/Blockchain.hpp"
#include "../src/MockChunk.hpp"
#include "../src/Chunk.hpp"
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