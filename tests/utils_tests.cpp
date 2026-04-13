#include <catch2/catch_all.hpp>
#include "../src/utils.hpp"

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
