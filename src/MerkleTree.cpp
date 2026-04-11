#include "MerkleTree.hpp"
#include "utils.hpp"
#include <openssl/evp.h>
#include <sstream>
#include <stdexcept>
#include <cstring>

namespace {

// Convert a 64-char hex string to raw 32 bytes
std::string hexToRaw(const std::string &hex) {
    if (hex.size() != 64) {
        throw std::invalid_argument("Expected 64-char hex string, got " + std::to_string(hex.size()));
    }
    std::string raw(32, '\0');
    for (size_t i = 0; i < 32; i++) {
        unsigned int byte;
        std::sscanf(hex.c_str() + 2 * i, "%02x", &byte);
        raw[i] = static_cast<char>(byte);
    }
    return raw;
}

// SHA-256 of raw bytes, returning hex string
std::string sha256Raw(const std::string &data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int lengthOfHash = 0;

    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) throw std::runtime_error("Failed to create EVP_MD_CTX");

    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to initialize SHA256 digest");
    }
    if (EVP_DigestUpdate(mdctx, data.data(), data.size()) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to update SHA256 digest");
    }
    if (EVP_DigestFinal_ex(mdctx, hash, &lengthOfHash) != 1) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to finalize SHA256 digest");
    }
    EVP_MD_CTX_free(mdctx);

    return bytesToHexString(hash, lengthOfHash);
}

// Hash an internal node: SHA-256(0x01 || left_raw_32bytes || right_raw_32bytes)
std::string hashInternal(const std::string &leftHex, const std::string &rightHex) {
    std::string leftRaw = hexToRaw(leftHex);
    std::string rightRaw = hexToRaw(rightHex);
    std::string data;
    data.push_back('\x01');
    data.append(leftRaw);
    data.append(rightRaw);
    return sha256Raw(data);
}

} // anonymous namespace

namespace MerkleTree {

std::string computeLeafHash(const std::string &serializedEntry) {
    // Prefix with 0x00 byte, then SHA-256
    std::string prefixed;
    prefixed.push_back('\x00');
    prefixed.append(serializedEntry);
    return sha256Raw(prefixed);
}

std::string computeMerkleRoot(const std::vector<std::string> &leafHashes) {
    if (leafHashes.empty()) {
        // SHA-256 of empty string
        return sha256Raw("");
    }
    if (leafHashes.size() == 1) {
        return leafHashes[0];
    }

    // Build tree bottom-up using raw 32-byte digests
    std::vector<std::string> currentLevel = leafHashes;

    while (currentLevel.size() > 1) {
        std::vector<std::string> nextLevel;
        // Duplicate last node on odd count
        if (currentLevel.size() % 2 != 0) {
            currentLevel.push_back(currentLevel.back());
        }
        for (size_t i = 0; i < currentLevel.size(); i += 2) {
            nextLevel.push_back(hashInternal(currentLevel[i], currentLevel[i + 1]));
        }
        currentLevel = std::move(nextLevel);
    }

    return currentLevel[0];
}

std::vector<MerkleProofElement> generateProof(
    const std::vector<std::string> &leafHashes, size_t targetIndex) {
    if (leafHashes.empty() || targetIndex >= leafHashes.size()) {
        throw std::out_of_range("Target index out of range");
    }

    if (leafHashes.size() == 1) {
        return {}; // Single entry: proof is empty
    }

    std::vector<MerkleProofElement> proof;
    std::vector<std::string> currentLevel = leafHashes;
    size_t idx = targetIndex;

    while (currentLevel.size() > 1) {
        // Duplicate last node on odd count
        if (currentLevel.size() % 2 != 0) {
            currentLevel.push_back(currentLevel.back());
        }

        if (idx % 2 == 0) {
            // Sibling is on the right
            proof.push_back({currentLevel[idx + 1], false});
        } else {
            // Sibling is on the left
            proof.push_back({currentLevel[idx - 1], true});
        }

        // Build next level
        std::vector<std::string> nextLevel;
        for (size_t i = 0; i < currentLevel.size(); i += 2) {
            nextLevel.push_back(hashInternal(currentLevel[i], currentLevel[i + 1]));
        }
        currentLevel = std::move(nextLevel);
        idx /= 2;
    }

    return proof;
}

bool verifyProof(const std::string &expectedRoot,
                 const std::string &leafHash,
                 const std::vector<MerkleProofElement> &proof) {
    std::string currentHash = leafHash;

    for (const auto &element : proof) {
        if (element.isLeft) {
            currentHash = hashInternal(element.hash, currentHash);
        } else {
            currentHash = hashInternal(currentHash, element.hash);
        }
    }

    return currentHash == expectedRoot;
}

} // namespace MerkleTree
