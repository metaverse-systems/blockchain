#include <catch2/catch_all.hpp>
#include "../src/StreamEntry.hpp"
#include <sstream>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

TEST_CASE("StreamEntry Boost.Serialization round-trip", "[StreamEntry]")
{
    StreamEntry original;
    original.stream = "assets";
    original.key = "item-42";
    original.data = R"({"name":"Sword","damage":50})";

    // Serialize
    std::ostringstream oss;
    {
        boost::archive::binary_oarchive oa(oss);
        oa << original;
    }

    // Deserialize
    StreamEntry restored;
    std::istringstream iss(oss.str());
    {
        boost::archive::binary_iarchive ia(iss);
        ia >> restored;
    }

    REQUIRE(restored.stream == original.stream);
    REQUIRE(restored.key == original.key);
    REQUIRE(restored.data == original.data);
}

TEST_CASE("isValidStreamName accepts valid names", "[StreamEntry][validation]")
{
    REQUIRE(isValidStreamName("assets"));
    REQUIRE(isValidStreamName("my-stream"));
    REQUIRE(isValidStreamName("my_stream_123"));
    REQUIRE(isValidStreamName("A"));
    REQUIRE(isValidStreamName(std::string(256, 'a'))); // max length
}

TEST_CASE("isValidStreamName rejects invalid names", "[StreamEntry][validation]")
{
    REQUIRE_FALSE(isValidStreamName(""));                      // empty
    REQUIRE_FALSE(isValidStreamName(std::string(257, 'a')));   // too long
    REQUIRE_FALSE(isValidStreamName("has space"));             // spaces
    REQUIRE_FALSE(isValidStreamName("has.dot"));               // dots
    REQUIRE_FALSE(isValidStreamName("has/slash"));             // slashes
    REQUIRE_FALSE(isValidStreamName("has@special"));           // special chars
}

TEST_CASE("isValidStreamEntry validates entries", "[StreamEntry][validation]")
{
    SECTION("Valid entry") {
        StreamEntry e;
        e.stream = "assets";
        e.key = "item-1";
        e.data = "some data";
        REQUIRE(isValidStreamEntry(e));
    }

    SECTION("Empty key is invalid") {
        StreamEntry e;
        e.stream = "assets";
        e.key = "";
        e.data = "data";
        REQUIRE_FALSE(isValidStreamEntry(e));
    }

    SECTION("Invalid stream name is invalid") {
        StreamEntry e;
        e.stream = "";
        e.key = "key";
        e.data = "data";
        REQUIRE_FALSE(isValidStreamEntry(e));
    }
}
