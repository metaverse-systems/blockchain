#pragma once

#include <vector>
#include <string>
#include <set>
#include <map>
#include <filesystem>
#include <iostream>
#include <chrono>
#include "Block.hpp"
#include "StreamEntry.hpp"
#include "ConsensusConfig.hpp"
#include "IChainReader.hpp"
#include "IChainWriter.hpp"
#include "json.hpp"

class IBlockchain : public IChainReader, public IChainWriter
{
  private:
    std::filesystem::path blockchainPath;
    bool shutting_down_ = false;
  public:
    const size_t chunkSize = 100;

    virtual ~IBlockchain() = default;

    bool isShuttingDown() const override { return shutting_down_; }
    void setShuttingDown() override { shutting_down_ = true; }
    
    // Persistence methods (not in sub-interfaces)
    virtual void loadChunk(size_t chunk_id) = 0;
    virtual void freeChunk(size_t chunk_id) = 0;
    virtual void saveChunk(size_t chunk_id) = 0;
    virtual void loadKeys() = 0;
    virtual void dumpBlocks() = 0;
    virtual void dumpKeys() = 0;

    static bool isValidNewBlock(const Block &newBlock, const Block &previousBlock, const ConsensusConfig &config)
    {
        if (previousBlock.index + 1 != newBlock.index) {
            logMessage("ERROR", "Invalid index: expected " + std::to_string(previousBlock.index + 1)
                       + " got " + std::to_string(newBlock.index));
            return false;
        }
        if (previousBlock.hash != newBlock.prevHash) {
            logMessage("ERROR", "Invalid previous hash for block " + std::to_string(newBlock.index));
            return false;
        }
        if (newBlock.calculateHash() != newBlock.hash) {
            logMessage("ERROR", "Invalid hash for block " + std::to_string(newBlock.index)
                       + ": computed " + newBlock.calculateHash() + " stored " + newBlock.hash);
            return false;
        }
        // Genesis block (index 0) is exempt from PoW checks
        if (newBlock.index > 0) {
            if (newBlock.difficulty < config.minDifficulty) {
                logMessage("ERROR", "Block " + std::to_string(newBlock.index)
                           + " difficulty " + std::to_string(newBlock.difficulty)
                           + " below minimum " + std::to_string(config.minDifficulty));
                return false;
            }
            if (!checkLeadingZeroBits(newBlock.hash, newBlock.difficulty)) {
                logMessage("ERROR", "Block " + std::to_string(newBlock.index)
                           + " hash does not meet difficulty " + std::to_string(newBlock.difficulty));
                return false;
            }
            auto now = static_cast<uint64_t>(
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));
            if (newBlock.timestamp > now + config.maxFutureTimestamp) {
                logMessage("ERROR", "Block " + std::to_string(newBlock.index)
                           + " timestamp too far in the future");
                return false;
            }

            // Validate stream entries in the block
            for (const auto &entry : newBlock.entries) {
                if (!isValidStreamName(entry.stream)) {
                    logMessage("ERROR", "Block " + std::to_string(newBlock.index)
                               + " contains entry with invalid stream name: " + entry.stream);
                    return false;
                }
                if (entry.key.empty()) {
                    logMessage("ERROR", "Block " + std::to_string(newBlock.index)
                               + " contains entry with empty key");
                    return false;
                }
                static constexpr size_t kMaxDataSize = 128ULL * 1024 * 1024;
                if (entry.data.size() > kMaxDataSize) {
                    logMessage("ERROR", "Block " + std::to_string(newBlock.index)
                               + " contains entry with oversized data: " + std::to_string(entry.data.size()));
                    return false;
                }
            }
        }
        return true;
    }
};
