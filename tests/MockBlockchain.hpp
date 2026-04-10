#pragma once

#include "../src/IBlockchain.hpp"
#include "../src/Block.hpp"
#include "../src/utils.hpp"
#include <vector>
#include <stdexcept>

class MockBlockchain : public IBlockchain {
public:
    std::vector<Block> blocks;
    std::vector<Block> appended_blocks;
    bool save_chunk_called = false;
    bool save_keys_called = false;

    MockBlockchain() {
        // Genesis block
        Block genesis(0, 0, "", "GENESIS", 0, 0);
        blocks.push_back(genesis);
    }

    void loadChunk(size_t) override {}
    void freeChunk(size_t) override {}
    void saveChunk(size_t) override { save_chunk_called = true; }
    void loadKeys() override {}
    void saveKeys() override { save_keys_called = true; }
    void dumpBlocks() override {}
    void dumpKeys() override {}
    void generateGenesisBlock() override {}

    Block addBlock(const std::string &data, const std::vector<std::string> &) override {
        auto &prev = blocks.back();
        Block b;
        b.index = prev.index + 1;
        b.timestamp = static_cast<uint64_t>(std::time(nullptr));
        b.data = data;
        b.prevHash = prev.hash;
        b.difficulty = 1;
        b.nonce = 0;
        b.hash = b.calculateHash();
        blocks.push_back(b);
        return b;
    }

    void appendBlock(const Block &block) override {
        blocks.push_back(block);
        appended_blocks.push_back(block);
    }

    std::vector<Block> getBlocksByKeys(const std::vector<std::string> &) override {
        return {};
    }

    Block getBlockByIndex(size_t index) override {
        if (index >= blocks.size()) {
            throw std::out_of_range("Block index out of range: " + std::to_string(index));
        }
        return blocks[index];
    }

    void replaceChain(const std::vector<Block> &candidateBlocks) override {
        blocks = candidateBlocks;
    }

    size_t getChainBlockCount() const override {
        return blocks.size();
    }

    // Helper to create a valid next block for testing (mines a nonce for PoW)
    Block createValidNextBlock(const std::string &data = "test") {
        auto &prev = blocks.back();
        Block b;
        b.index = prev.index + 1;
        b.timestamp = static_cast<uint64_t>(std::time(nullptr));
        b.data = data;
        b.prevHash = prev.hash;
        b.difficulty = 1;
        b.nonce = 0;
        b.hash = b.calculateHash();
        while (!checkLeadingZeroBits(b.hash, b.difficulty)) {
            b.nonce++;
            b.hash = b.calculateHash();
        }
        return b;
    }
};
