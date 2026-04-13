#include <catch2/catch_all.hpp>
#include "../src/MerkleProofService.hpp"
#include "../src/MerkleTree.hpp"
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include <sstream>
#include <boost/archive/binary_oarchive.hpp>

namespace {

Block make_block_with_entries(size_t index, size_t numEntries) {
    Block b;
    b.index = index;
    b.timestamp = 1000;
    b.difficulty = 0;
    b.nonce = 0;
    b.prevHash = "";

    std::vector<std::string> leafHashes;
    for (size_t i = 0; i < numEntries; i++) {
        StreamEntry e;
        e.stream = "test";
        e.key = "k" + std::to_string(i);
        e.data = "data" + std::to_string(i);
        b.entries.push_back(e);

        std::ostringstream oss;
        boost::archive::binary_oarchive oa(oss);
        oa << e;
        leafHashes.push_back(MerkleTree::computeLeafHash(oss.str()));
    }

    b.merkleRoot = MerkleTree::computeMerkleRoot(leafHashes);
    b.hash = b.calculateHash();
    return b;
}

} // anonymous namespace

TEST_CASE("MerkleProofService getInclusionProof with single entry", "[MerkleProofService]")
{
    MerkleProofService service;
    Block block = make_block_with_entries(0, 1);

    auto result = service.getInclusionProof(block, 0);

    REQUIRE(result["blockIndex"] == 0);
    REQUIRE(result["entryIndex"] == 0);
    REQUIRE(result["merkleRoot"] == block.merkleRoot);
    REQUIRE(result.contains("leafHash"));
    REQUIRE(result.contains("proof"));
}

TEST_CASE("MerkleProofService getInclusionProof with multiple entries", "[MerkleProofService]")
{
    MerkleProofService service;
    Block block = make_block_with_entries(5, 4);

    for (size_t i = 0; i < 4; i++) {
        auto result = service.getInclusionProof(block, i);
        REQUIRE(result["blockIndex"] == 5);
        REQUIRE(result["entryIndex"] == i);
        REQUIRE(result["merkleRoot"] == block.merkleRoot);
        REQUIRE(!result["leafHash"].get<std::string>().empty());
    }
}

TEST_CASE("MerkleProofService getInclusionProof out-of-bounds throws", "[MerkleProofService]")
{
    MerkleProofService service;
    Block block = make_block_with_entries(0, 2);

    REQUIRE_THROWS_AS(service.getInclusionProof(block, 2), std::out_of_range);
    REQUIRE_THROWS_AS(service.getInclusionProof(block, 100), std::out_of_range);
}

TEST_CASE("MerkleProofService verifyInclusionProof valid proof", "[MerkleProofService]")
{
    MerkleProofService service;
    Block block = make_block_with_entries(0, 3);

    auto proof = service.getInclusionProof(block, 1);

    auto verification = service.verifyInclusionProof(
        block, proof["leafHash"].get<std::string>(), proof["proof"]);

    REQUIRE(verification["valid"] == true);
    REQUIRE(verification["merkleRoot"] == block.merkleRoot);
}

TEST_CASE("MerkleProofService verifyInclusionProof tampered proof", "[MerkleProofService]")
{
    MerkleProofService service;
    Block block = make_block_with_entries(0, 3);

    auto proof = service.getInclusionProof(block, 0);

    // Tamper with the leaf hash
    std::string tamperedLeaf = "0000000000000000000000000000000000000000000000000000000000000000";

    auto verification = service.verifyInclusionProof(block, tamperedLeaf, proof["proof"]);

    REQUIRE(verification["valid"] == false);
}

TEST_CASE("MerkleProofService round-trip proof generation and verification", "[MerkleProofService]")
{
    MerkleProofService service;
    Block block = make_block_with_entries(10, 5);

    // Verify all entries have valid proofs
    for (size_t i = 0; i < 5; i++) {
        auto proof = service.getInclusionProof(block, i);
        auto verification = service.verifyInclusionProof(
            block, proof["leafHash"].get<std::string>(), proof["proof"]);
        REQUIRE(verification["valid"] == true);
    }
}
