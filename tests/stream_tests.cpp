#include <catch2/catch_all.hpp>
#include "../src/Blockchain.hpp"
#include "../src/MockChunk.hpp"
#include "../src/StreamEntry.hpp"

// --- Stream Index and Query Tests (US2: T016) ---

TEST_CASE("getStreamEntries returns all entries for stream+key in chain order", "[Blockchain][streams]")
{
    Blockchain<MockChunk> bc(".");

    bc.publish("assets", "item-42", "v1", {});
    bc.publish("assets", "item-42", "v2", {});
    bc.publish("assets", "item-42", "v3", {});

    auto entries = bc.getStreamEntries("assets", "item-42");
    REQUIRE(entries.size() == 3);
    REQUIRE(entries[0].second.data == "v1");
    REQUIRE(entries[1].second.data == "v2");
    REQUIRE(entries[2].second.data == "v3");
    // Verify chain order
    REQUIRE(entries[0].first < entries[1].first);
    REQUIRE(entries[1].first < entries[2].first);
}

TEST_CASE("getStreamEntries returns all entries in a stream when key omitted", "[Blockchain][streams]")
{
    Blockchain<MockChunk> bc(".");

    bc.publish("logs", "event-1", "login", {});
    bc.publish("logs", "event-2", "logout", {});
    bc.publish("other", "x", "data", {});

    auto entries = bc.getStreamEntries("logs");
    REQUIRE(entries.size() == 2);
}

TEST_CASE("getStreamEntry returns latest entry only", "[Blockchain][streams]")
{
    Blockchain<MockChunk> bc(".");

    bc.publish("assets", "item-42", "v1", {});
    bc.publish("assets", "item-42", "v2", {});

    auto [blockIdx, entry] = bc.getStreamEntry("assets", "item-42");
    REQUIRE(entry.data == "v2");
    REQUIRE(blockIdx == 2); // genesis=0, first publish=1, second=2
}

TEST_CASE("getStreamEntry throws for nonexistent stream+key", "[Blockchain][streams]")
{
    Blockchain<MockChunk> bc(".");

    REQUIRE_THROWS_AS(bc.getStreamEntry("nonexistent", "key"), std::runtime_error);
}

TEST_CASE("getBlockByIndex response includes entries", "[Blockchain][streams]")
{
    Blockchain<MockChunk> bc(".");

    bc.publish("assets", "item-1", "sword", {});

    Block b = bc.getBlockByIndex(1);
    REQUIRE(b.entries.size() == 1);
    REQUIRE(b.entries[0].stream == "assets");
    REQUIRE(b.entries[0].key == "item-1");
    REQUIRE(b.entries[0].data == "sword");
}

// --- Stream Management Tests (US4: T021) ---

TEST_CASE("Explicit createStream success", "[Blockchain][streams]")
{
    Blockchain<MockChunk> bc(".");

    bc.createStream("inventory");
    auto streams = bc.listStreams();
    REQUIRE(streams.count("inventory") == 1);
}

TEST_CASE("Duplicate createStream throws", "[Blockchain][streams]")
{
    Blockchain<MockChunk> bc(".");

    bc.createStream("inventory");
    REQUIRE_THROWS_AS(bc.createStream("inventory"), std::runtime_error);
}

TEST_CASE("listStreams returns all streams sorted", "[Blockchain][streams]")
{
    Blockchain<MockChunk> bc(".");

    bc.createStream("zebra");
    bc.createStream("alpha");
    bc.publish("middle", "k", "d", {});

    auto streams = bc.listStreams();
    REQUIRE(streams.size() == 3);
    // std::set is already sorted
    auto it = streams.begin();
    REQUIRE(*it++ == "alpha");
    REQUIRE(*it++ == "middle");
    REQUIRE(*it++ == "zebra");
}

TEST_CASE("Auto-created stream via publish appears in listStreams", "[Blockchain][streams]")
{
    Blockchain<MockChunk> bc(".");

    bc.publish("auto-stream", "k", "d", {});
    auto streams = bc.listStreams();
    REQUIRE(streams.count("auto-stream") == 1);
}

// --- Stream Permission Tests (US5: T025) ---
// Permission check is in RPC handler, not Blockchain.
// These tests verify the Blockchain layer doesn't enforce permissions.

TEST_CASE("Blockchain publish does not enforce stream permissions", "[Blockchain][streams][permissions]")
{
    Blockchain<MockChunk> bc(".");

    // Should succeed regardless — permissions are RPC-level only
    auto b = bc.publish("restricted", "k", "d", {});
    REQUIRE(b.entries.size() == 1);
    REQUIRE(b.entries[0].stream == "restricted");
}
