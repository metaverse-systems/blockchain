#pragma once
#include <vector>
#include <map>
#include "block.hpp"
#include "IChunk.hpp"
#include "IBlockchain.hpp"
#include <filesystem>

template<typename ChunkHandler>
class blockchain : public IBlockchain
{
  private:
    std::vector<ChunkHandler> chain;
    std::map<std::string, std::vector<size_t>> keyIndexMap;
    bool isValidNewBlock(const block &newBlock, const block &previousBlock);
    std::filesystem::path blockchainPath;
  public:
    
    blockchain(std::filesystem::path path): blockchainPath(path)
    {
        this->generateGenesisBlock();
    };
    void generateGenesisBlock();
    block addBlock(const std::string &data, const std::vector<std::string> &keys);
    std::vector<block> getBlocksByKeys(const std::vector<std::string> &keys);
    auto getBlockByIndex(size_t index) -> block;
    void dumpBlocks();
    void dumpKeys();
    void saveChunk(size_t chunkIndex);
    void loadChunk(size_t chunkIndex);
    void freeChunk(size_t chunkIndex);
    void saveKeys();
    void loadKeys();
};