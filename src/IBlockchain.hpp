#pragma once

#include <vector>
#include <string>
#include <filesystem>
#include <iostream>
#include "Block.hpp"

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
    virtual std::vector<Block> getBlocksByKeys(const std::vector<std::string> &keys) = 0;
    virtual Block getBlockByIndex(size_t index) = 0;
    static bool isValidNewBlock(const Block &newBlock, const Block &previousBlock)
    {
        if (previousBlock.index + 1 != newBlock.index) {
            std::cerr << "Invalid index" << std::endl;
            return false;
        } else if (previousBlock.hash != newBlock.prevHash) {
            std::cerr << "Invalid previous hash" << std::endl;
            return false;
        } else if (newBlock.calculateHash() != newBlock.hash) {
            std::cerr << "Invalid hash: " << newBlock.calculateHash() << " " << newBlock.hash << std::endl;
            return false;
        }
        return true;
    }
};
