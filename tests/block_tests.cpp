#include <catch2/catch_all.hpp>
#include "../src/block.hpp"

TEST_CASE("Block Construction", "[block]")
{
    // Arrange
    size_t index = 0;
    uint64_t time = 0;
    std::string prev_hash = "0";
    std::string data = "";

    // Act
    block b(index, time, prev_hash, data);

    // Assert
    REQUIRE(b.index == index);
    REQUIRE(b.timestamp == time);
    REQUIRE(b.prevHash == prev_hash);
    REQUIRE(b.data == data);
    REQUIRE(!b.hash.empty()); // Hash should not be empty
}