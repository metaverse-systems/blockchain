#pragma once

#include "../src/IBlockchain.hpp"
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/utils.hpp"
#include <vector>
#include <set>
#include <map>
#include <stdexcept>

class MockBlockchain : public IBlockchain {
public:
    std::vector<Block> blocks;
    std::vector<Block> appended_blocks;
    bool save_chunk_called = false;
    bool save_keys_called = false;
    std::set<std::string> streamRegistry;
    std::map<std::string, std::map<std::string, std::vector<size_t>>> streamKeyIndex;

    MockBlockchain() {
        // Genesis block
        Block genesis(0, 0, "", {}, 0, 0);
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

    Block publish(const std::string &stream, const std::string &key,
                  const std::string &data, const std::vector<std::string> &) override {
        streamRegistry.insert(stream);

        StreamEntry entry;
        entry.stream = stream;
        entry.key = key;
        entry.data = data;

        auto &prev = blocks.back();
        Block b;
        b.index = prev.index + 1;
        b.timestamp = static_cast<uint64_t>(std::time(nullptr));
        b.entries = {entry};
        b.prevHash = prev.hash;
        b.difficulty = 1;
        b.nonce = 0;
        b.hash = b.calculateHash();
        blocks.push_back(b);

        streamKeyIndex[stream][key].push_back(b.index);
        return b;
    }

    void createStream(const std::string &name) override {
        if (streamRegistry.count(name)) {
            throw std::runtime_error("Stream already exists: " + name);
        }
        streamRegistry.insert(name);
    }

    std::set<std::string> listStreams() const override {
        return streamRegistry;
    }

    std::vector<std::pair<size_t, StreamEntry>> getStreamEntries(
        const std::string &stream, const std::string &key = "") const override {
        std::vector<std::pair<size_t, StreamEntry>> results;
        auto streamIt = streamKeyIndex.find(stream);
        if (streamIt == streamKeyIndex.end()) return results;

        if (!key.empty()) {
            auto keyIt = streamIt->second.find(key);
            if (keyIt == streamIt->second.end()) return results;
            for (auto idx : keyIt->second) {
                if (idx < blocks.size()) {
                    for (const auto &e : blocks[idx].entries) {
                        if (e.stream == stream && e.key == key) {
                            results.emplace_back(idx, e);
                        }
                    }
                }
            }
        } else {
            for (const auto &[k, indices] : streamIt->second) {
                for (auto idx : indices) {
                    if (idx < blocks.size()) {
                        for (const auto &e : blocks[idx].entries) {
                            if (e.stream == stream) {
                                results.emplace_back(idx, e);
                            }
                        }
                    }
                }
            }
        }
        return results;
    }

    std::pair<size_t, StreamEntry> getStreamEntry(
        const std::string &stream, const std::string &key) const override {
        auto streamIt = streamKeyIndex.find(stream);
        if (streamIt == streamKeyIndex.end()) throw std::runtime_error("Entry not found");
        auto keyIt = streamIt->second.find(key);
        if (keyIt == streamIt->second.end() || keyIt->second.empty())
            throw std::runtime_error("Entry not found");
        size_t lastIdx = keyIt->second.back();
        for (auto it = blocks[lastIdx].entries.rbegin(); it != blocks[lastIdx].entries.rend(); ++it) {
            if (it->stream == stream && it->key == key) return {lastIdx, *it};
        }
        throw std::runtime_error("Entry not found");
    }

    void appendBlock(const Block &block) override {
        blocks.push_back(block);
        appended_blocks.push_back(block);
        for (const auto &entry : block.entries) {
            streamRegistry.insert(entry.stream);
            streamKeyIndex[entry.stream][entry.key].push_back(block.index);
        }
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

    size_t getChainLength() const override {
        return blocks.size();
    }

    size_t getChunkCount() const override {
        return (blocks.size() + chunkSize - 1) / chunkSize;
    }

    // Helper to create a valid next block for testing (mines a nonce for PoW)
    Block createValidNextBlock(const std::string &data = "test") {
        StreamEntry entry;
        entry.stream = "test";
        entry.key = "test";
        entry.data = data;

        auto &prev = blocks.back();
        Block b;
        b.index = prev.index + 1;
        b.timestamp = static_cast<uint64_t>(std::time(nullptr));
        b.entries = {entry};
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
