#include <catch2/catch_all.hpp>
#include "../src/MerkleTree.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/utils.hpp"
#include <sstream>
#include <cmath>
#include <boost/archive/binary_oarchive.hpp>

// Helper: serialize a StreamEntry to binary string
static std::string serializeEntry(const StreamEntry &entry) {
    std::ostringstream oss;
    boost::archive::binary_oarchive oa(oss);
    oa << entry;
    return oss.str();
}

// Helper: create a leaf hash for a StreamEntry
static std::string leafHashFor(const std::string &stream, const std::string &key, const std::string &data) {
    StreamEntry e;
    e.stream = stream;
    e.key = key;
    e.data = data;
    return MerkleTree::computeLeafHash(serializeEntry(e));
}

// ============================================================
// T008: Merkle tree unit tests
// ============================================================

TEST_CASE("computeLeafHash is deterministic", "[merkle][US1]") {
    StreamEntry e;
    e.stream = "test";
    e.key = "k1";
    e.data = "hello";
    std::string serialized = serializeEntry(e);

    std::string hash1 = MerkleTree::computeLeafHash(serialized);
    std::string hash2 = MerkleTree::computeLeafHash(serialized);

    REQUIRE(hash1 == hash2);
    REQUIRE(hash1.size() == 64); // SHA-256 hex
}

TEST_CASE("computeMerkleRoot with 0 entries returns SHA-256 of empty string", "[merkle][US1]") {
    std::string root = MerkleTree::computeMerkleRoot({});
    // SHA-256("") is a well-known constant
    REQUIRE(root == sha256(""));
    // Verify it's the known constant
    REQUIRE(root == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("computeMerkleRoot with 1 entry equals leaf hash", "[merkle][US1]") {
    std::string leaf = leafHashFor("s", "k", "d");
    std::string root = MerkleTree::computeMerkleRoot({leaf});
    REQUIRE(root == leaf);
}

TEST_CASE("computeMerkleRoot with 2 entries", "[merkle][US1]") {
    std::string leaf1 = leafHashFor("s", "k1", "d1");
    std::string leaf2 = leafHashFor("s", "k2", "d2");
    std::string root = MerkleTree::computeMerkleRoot({leaf1, leaf2});
    REQUIRE(root.size() == 64);
    REQUIRE(root != leaf1);
    REQUIRE(root != leaf2);
}

TEST_CASE("computeMerkleRoot with 5 entries (odd count)", "[merkle][US1]") {
    std::vector<std::string> leaves;
    for (int i = 0; i < 5; i++) {
        leaves.push_back(leafHashFor("s", "k" + std::to_string(i), "d" + std::to_string(i)));
    }
    std::string root = MerkleTree::computeMerkleRoot(leaves);
    REQUIRE(root.size() == 64);
}

TEST_CASE("computeMerkleRoot with even number of entries", "[merkle][US1]") {
    std::vector<std::string> leaves;
    for (int i = 0; i < 4; i++) {
        leaves.push_back(leafHashFor("s", "k" + std::to_string(i), "d" + std::to_string(i)));
    }
    std::string root = MerkleTree::computeMerkleRoot(leaves);
    REQUIRE(root.size() == 64);
}

TEST_CASE("computeMerkleRoot is order sensitive", "[merkle][US1]") {
    std::string leaf1 = leafHashFor("s", "k1", "d1");
    std::string leaf2 = leafHashFor("s", "k2", "d2");

    std::string root1 = MerkleTree::computeMerkleRoot({leaf1, leaf2});
    std::string root2 = MerkleTree::computeMerkleRoot({leaf2, leaf1});

    REQUIRE(root1 != root2);
}

TEST_CASE("computeMerkleRoot with duplicate entries", "[merkle][US1]") {
    std::string leaf = leafHashFor("s", "k1", "d1");
    std::string root = MerkleTree::computeMerkleRoot({leaf, leaf});
    REQUIRE(root.size() == 64);
    // With two identical leaves, root should still differ from the leaf
    // (because internal node uses 0x01 prefix)
    REQUIRE(root != leaf);
}

// ============================================================
// T009: Merkle tree edge-case tests
// ============================================================

TEST_CASE("Single entry root equals leaf hash (edge case)", "[merkle][US1][edge]") {
    std::string leaf = leafHashFor("stream", "key", "value");
    std::string root = MerkleTree::computeMerkleRoot({leaf});
    REQUIRE(root == leaf);
}

TEST_CASE("Odd-count duplication produces correct root", "[merkle][US1][edge]") {
    // 3 leaves: last node should be duplicated
    std::string l1 = leafHashFor("s", "k1", "d1");
    std::string l2 = leafHashFor("s", "k2", "d2");
    std::string l3 = leafHashFor("s", "k3", "d3");

    std::string root3 = MerkleTree::computeMerkleRoot({l1, l2, l3});
    REQUIRE(root3.size() == 64);

    // Adding the same 3rd leaf (simulating duplication manually) should give a different tree
    // than just 3 leaves, because the tree structure differs
    std::string root4 = MerkleTree::computeMerkleRoot({l1, l2, l3, l3});
    // With duplication of last node on odd count at level 1, the 3-leaf tree
    // should produce a specific root that differs from a 4-leaf tree with l3 duplicated
    // (because the duplication happens at the leaf level for 3, but for 4 it's even)
    // Actually: 3 leaves → duplicate l3 → 4 leaves: {l1, l2, l3, l3} which IS the same
    REQUIRE(root3 == root4);
}

TEST_CASE("Large entry count (100 entries)", "[merkle][US1][edge]") {
    std::vector<std::string> leaves;
    for (int i = 0; i < 100; i++) {
        leaves.push_back(leafHashFor("s", "k" + std::to_string(i), "data" + std::to_string(i)));
    }
    std::string root = MerkleTree::computeMerkleRoot(leaves);
    REQUIRE(root.size() == 64);
}

TEST_CASE("Large individual entry payload (>=1 MB)", "[merkle][US1][edge]") {
    StreamEntry e;
    e.stream = "big";
    e.key = "large";
    e.data = std::string(1024 * 1024, 'X'); // 1 MB
    std::string serialized = serializeEntry(e);
    std::string hash = MerkleTree::computeLeafHash(serialized);
    REQUIRE(hash.size() == 64);

    // Verify it's deterministic
    REQUIRE(hash == MerkleTree::computeLeafHash(serialized));
}

// ============================================================
// T015: Proof generation/verification unit tests
// ============================================================

TEST_CASE("generateProof and verifyProof for each position in multi-entry block", "[merkle][US2]") {
    std::vector<std::string> leaves;
    for (int i = 0; i < 8; i++) {
        leaves.push_back(leafHashFor("s", "k" + std::to_string(i), "d" + std::to_string(i)));
    }
    std::string root = MerkleTree::computeMerkleRoot(leaves);

    for (size_t i = 0; i < leaves.size(); i++) {
        auto proof = MerkleTree::generateProof(leaves, i);
        REQUIRE(MerkleTree::verifyProof(root, leaves[i], proof));
    }
}

TEST_CASE("verifyProof fails with tampered leaf", "[merkle][US2]") {
    std::vector<std::string> leaves;
    for (int i = 0; i < 4; i++) {
        leaves.push_back(leafHashFor("s", "k" + std::to_string(i), "d" + std::to_string(i)));
    }
    std::string root = MerkleTree::computeMerkleRoot(leaves);
    auto proof = MerkleTree::generateProof(leaves, 0);

    std::string tamperedLeaf = leafHashFor("s", "k0", "TAMPERED");
    REQUIRE_FALSE(MerkleTree::verifyProof(root, tamperedLeaf, proof));
}

TEST_CASE("verifyProof fails with truncated proof", "[merkle][US2]") {
    std::vector<std::string> leaves;
    for (int i = 0; i < 4; i++) {
        leaves.push_back(leafHashFor("s", "k" + std::to_string(i), "d" + std::to_string(i)));
    }
    std::string root = MerkleTree::computeMerkleRoot(leaves);
    auto proof = MerkleTree::generateProof(leaves, 0);

    // Remove last element
    auto truncated = proof;
    truncated.pop_back();
    REQUIRE_FALSE(MerkleTree::verifyProof(root, leaves[0], truncated));
}

TEST_CASE("verifyProof fails with extra element", "[merkle][US2]") {
    std::vector<std::string> leaves;
    for (int i = 0; i < 4; i++) {
        leaves.push_back(leafHashFor("s", "k" + std::to_string(i), "d" + std::to_string(i)));
    }
    std::string root = MerkleTree::computeMerkleRoot(leaves);
    auto proof = MerkleTree::generateProof(leaves, 0);

    // Add an extra element
    auto extended = proof;
    extended.push_back({leaves[1], true});
    REQUIRE_FALSE(MerkleTree::verifyProof(root, leaves[0], extended));
}

TEST_CASE("Proof size equals ceil(log2(n)) for various block sizes", "[merkle][US2]") {
    for (size_t n : {1, 10, 100}) {
        std::vector<std::string> leaves;
        for (size_t i = 0; i < n; i++) {
            leaves.push_back(leafHashFor("s", "k" + std::to_string(i), "d" + std::to_string(i)));
        }

        if (n == 1) {
            auto proof = MerkleTree::generateProof(leaves, 0);
            REQUIRE(proof.size() == 0);
        } else {
            auto proof = MerkleTree::generateProof(leaves, 0);
            size_t expectedDepth = static_cast<size_t>(std::ceil(std::log2(static_cast<double>(n))));
            REQUIRE(proof.size() == expectedDepth);
        }
    }
}

// ============================================================
// T016: Proof edge-case tests
// ============================================================

TEST_CASE("Proof for single-entry block (empty proof path)", "[merkle][US2][edge]") {
    std::string leaf = leafHashFor("s", "k", "d");
    std::string root = MerkleTree::computeMerkleRoot({leaf});
    auto proof = MerkleTree::generateProof({leaf}, 0);

    REQUIRE(proof.empty());
    REQUIRE(MerkleTree::verifyProof(root, leaf, proof));
}

TEST_CASE("Proof for entry at last position in odd-count block", "[merkle][US2][edge]") {
    std::vector<std::string> leaves;
    for (int i = 0; i < 5; i++) {
        leaves.push_back(leafHashFor("s", "k" + std::to_string(i), "d" + std::to_string(i)));
    }
    std::string root = MerkleTree::computeMerkleRoot(leaves);

    auto proof = MerkleTree::generateProof(leaves, 4); // last position
    REQUIRE(MerkleTree::verifyProof(root, leaves[4], proof));
}

TEST_CASE("Out-of-range index returns error", "[merkle][US2][edge]") {
    std::vector<std::string> leaves = {leafHashFor("s", "k", "d")};
    REQUIRE_THROWS_AS(MerkleTree::generateProof(leaves, 1), std::out_of_range);
    REQUIRE_THROWS_AS(MerkleTree::generateProof({}, 0), std::out_of_range);
}
