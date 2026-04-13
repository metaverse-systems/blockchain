#pragma once

#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/ConsensusConfig.hpp"
#include "../src/MerkleTree.hpp"
#include "../src/utils.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <sstream>
#include <boost/archive/binary_oarchive.hpp>

namespace TestHelpers {

inline std::filesystem::path createTestDir(const std::string &name) {
    auto dir = std::filesystem::temp_directory_path() / ("test_" + name);
    std::filesystem::create_directories(dir);
    return dir;
}

inline void cleanupTestDir(const std::filesystem::path &dir) {
    std::filesystem::remove_all(dir);
}

inline ConsensusConfig defaultConsensusConfig() {
    ConsensusConfig cfg;
    cfg.initialDifficulty = 0;
    cfg.minDifficulty = 0;
    cfg.miningTimeout = 60;
    return cfg;
}

inline Block mineTestBlock(size_t index, uint64_t timestamp, const std::string &prevHash,
                           const std::string &data, uint32_t difficulty) {
    StreamEntry entry;
    entry.stream = "test";
    entry.key = "k";
    entry.data = data;

    std::vector<std::string> leafHashes;
    {
        std::ostringstream oss;
        boost::archive::binary_oarchive oa(oss);
        oa << entry;
        leafHashes.push_back(MerkleTree::computeLeafHash(oss.str()));
    }

    Block b;
    b.index = index;
    b.timestamp = timestamp;
    b.entries = {entry};
    b.prevHash = prevHash;
    b.difficulty = difficulty;
    b.nonce = 0;
    b.merkleRoot = MerkleTree::computeMerkleRoot(leafHashes);
    b.hash = b.calculateHash();
    while (!checkLeadingZeroBits(b.hash, difficulty)) {
        b.nonce++;
        b.hash = b.calculateHash();
    }
    return b;
}

inline std::vector<Block> buildValidChain(size_t length, uint32_t difficulty = 0) {
    std::vector<Block> chain;
    Block genesis(0, 0, "", {}, 0, 0);
    chain.push_back(genesis);

    for (size_t i = 1; i < length; i++) {
        chain.push_back(mineTestBlock(i, static_cast<uint64_t>(i * 10),
                                      chain.back().hash, "data_" + std::to_string(i), difficulty));
    }
    return chain;
}

// Fast difficulty-0 block creation for chunk persistence tests
inline Block make_block(size_t index, const std::string &prevHash) {
    StreamEntry e;
    e.stream = "test";
    e.key = "k" + std::to_string(index);
    e.data = "data";

    Block b;
    b.index = index;
    b.timestamp = static_cast<uint64_t>(std::time(nullptr));
    b.entries = {e};
    b.prevHash = prevHash;
    b.difficulty = 0;
    b.nonce = 0;
    b.hash = b.calculateHash();
    return b;
}

} // namespace TestHelpers
