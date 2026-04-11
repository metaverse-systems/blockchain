#pragma once

#include <string>
#include <vector>
#include <cstdint>

struct MerkleProofElement {
    std::string hash;
    bool isLeft;
};

namespace MerkleTree {
    // Compute the leaf hash for a StreamEntry: SHA-256(0x00 || Boost-serialized bytes)
    std::string computeLeafHash(const std::string &serializedEntry);

    // Compute the Merkle root from a vector of leaf hex hashes
    // Returns SHA-256 of empty string for zero entries
    std::string computeMerkleRoot(const std::vector<std::string> &leafHashes);

    // Generate a proof-of-inclusion for the leaf at targetIndex
    std::vector<MerkleProofElement> generateProof(
        const std::vector<std::string> &leafHashes, size_t targetIndex);

    // Verify a proof against an expected root
    bool verifyProof(const std::string &expectedRoot,
                     const std::string &leafHash,
                     const std::vector<MerkleProofElement> &proof);
}
