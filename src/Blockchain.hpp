#pragma once
#include <vector>
#include <map>
#include "Block.hpp"
#include "IChunk.hpp"
#include "IBlockchain.hpp"
#include <filesystem>

template<typename ChunkHandler>
class Blockchain : public IBlockchain
{
  private:
    std::vector<ChunkHandler> chain;
    std::map<std::string, std::vector<size_t>> keyIndexMap;
    bool isValidNewBlock(const Block &newBlock, const Block &previousBlock);
    std::filesystem::path blockchainPath;
  public:
    
    Blockchain(std::filesystem::path path): blockchainPath(path)
    {
        this->generateGenesisBlock();
    };
    void generateGenesisBlock();
    Block addBlock(const std::string &data, const std::vector<std::string> &keys);
    std::vector<Block> getBlocksByKeys(const std::vector<std::string> &keys);
    auto getBlockByIndex(size_t index) -> Block;
    void dumpBlocks();
    void dumpKeys();
    void saveChunk(size_t chunkIndex);
    void loadChunk(size_t chunkIndex);
    void freeChunk(size_t chunkIndex);
    void saveKeys();
    void loadKeys();
};