#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"

TEST_CASE("Block Construction", "[Block]")
{
    // Arrange
    size_t index = 0;
    uint64_t time = 0;
    std::string prev_hash = "0";
    std::string data = "";

    // Act
    Block b(index, time, prev_hash, data);

    // Assert
    REQUIRE(b.index == index);
    REQUIRE(b.timestamp == time);
    REQUIRE(b.prevHash == prev_hash);
    REQUIRE(b.data == data);
    REQUIRE(!b.hash.empty()); // Hash should not be empty
}