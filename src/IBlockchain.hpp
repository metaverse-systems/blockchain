#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include <chrono>
#include "Block.hpp"
#include "ConsensusConfig.hpp"

class IBlockchain
{
  private:
    std::filesystem::path blockchainPath;
  public:
    const size_t chunkSize = 100;

    virtual ~IBlockchain() = default;
    
    virtual void loadChunk(size_t chunk_id) = 0;
    virtual void freeChunk(size_t chunk_id) = 0;
    virtual void saveChunk(size_t chunk_id) = 0;
    virtual void loadKeys() = 0;
    virtual void saveKeys() = 0;
    virtual void dumpBlocks() = 0;
    virtual void dumpKeys() = 0;
    virtual void generateGenesisBlock() = 0;
    virtual Block addBlock(const std::string &data, const std::vector<std::string> &keys) = 0;
    virtual void appendBlock(const Block &block) = 0;
    virtual std::vector<Block> getBlocksByKeys(const std::vector<std::string> &keys) = 0;
    virtual Block getBlockByIndex(size_t index) = 0;
    virtual void replaceChain(const std::vector<Block> &candidateBlocks) = 0;
    virtual size_t getChainBlockCount() const = 0;

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
        }
        return true;
    }
};
