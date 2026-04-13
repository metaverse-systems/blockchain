#include <catch2/catch_all.hpp>
#include "../src/utils.hpp"
#include "../src/network/PacketSerializer.hpp"
#include "../src/Block.hpp"
#include <boost/archive/binary_iarchive.hpp>
#include <sstream>

TEST_CASE("parsePeerKey rejects port 0", "[utils][parsePeerKey]") {
    REQUIRE_THROWS_AS(parsePeerKey("host:0"), std::invalid_argument);
}

TEST_CASE("parsePeerKey rejects port 70000", "[utils][parsePeerKey]") {
    REQUIRE_THROWS_AS(parsePeerKey("host:70000"), std::invalid_argument);
}

TEST_CASE("parsePeerKey rejects negative port", "[utils][parsePeerKey]") {
    REQUIRE_THROWS_AS(parsePeerKey("host:-1"), std::invalid_argument);
}

TEST_CASE("parsePeerKey rejects non-numeric port", "[utils][parsePeerKey]") {
    REQUIRE_THROWS_AS(parsePeerKey("host:abc"), std::invalid_argument);
}

TEST_CASE("parsePeerKey accepts IPv6 with valid port", "[utils][parsePeerKey]") {
    auto [host, port] = parsePeerKey("[::1]:8333");
    REQUIRE(host == "::1");
    REQUIRE(port == 8333);
}

TEST_CASE("parsePeerKey accepts IPv4 with max port", "[utils][parsePeerKey]") {
    auto [host, port] = parsePeerKey("192.168.1.1:65535");
    REQUIRE(host == "192.168.1.1");
    REQUIRE(port == 65535);
}

TEST_CASE("parsePeerKey accepts hostname with port 1", "[utils][parsePeerKey]") {
    auto [host, port] = parsePeerKey("host:1");
    REQUIRE(host == "host");
    REQUIRE(port == 1);
}

TEST_CASE("serialize_packet produces correct header and payload", "[utils][PacketSerializer]") {
    Block b;
    b.index = 42;
    b.prevHash = "abc";

    constexpr uint64_t ptype = 99;
    auto [header_data, payload] = serialize_packet(b, ptype);

    REQUIRE(header_data.size() == sizeof(PacketHeader));

    PacketHeader hdr;
    std::memcpy(&hdr, header_data.data(), sizeof(hdr));
    REQUIRE(hdr.type == ptype);
    REQUIRE(hdr.length == payload.size());

    // Verify payload deserializes back to an equivalent Block
    std::istringstream iss(payload);
    boost::archive::binary_iarchive ia(iss);
    Block restored;
    ia >> restored;
    REQUIRE(restored.index == b.index);
    REQUIRE(restored.prevHash == b.prevHash);
}
