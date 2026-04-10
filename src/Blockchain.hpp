#pragma once
#include <vector>
#include <map>
#include "Block.hpp"
#include "IChunk.hpp"
#include "IBlockchain.hpp"
#include "ConsensusConfig.hpp"
#include <filesystem>

template<typename ChunkHandler>
class Blockchain : public IBlockchain
{
  private:
    std::vector<ChunkHandler> chain;
    std::map<std::string, std::vector<size_t>> keyIndexMap;
    std::filesystem::path blockchainPath;
    ConsensusConfig config;
    uint32_t currentDifficulty;
  public:
    
    Blockchain(std::filesystem::path path, ConsensusConfig cfg = ConsensusConfig())
        : blockchainPath(path), config(cfg), currentDifficulty(cfg.initialDifficulty)
    {
        this->generateGenesisBlock();
    };
    void generateGenesisBlock();
    Block addBlock(const std::string &data, const std::vector<std::string> &keys);
    void appendBlock(const Block &block);
    std::vector<Block> getBlocksByKeys(const std::vector<std::string> &keys);
    auto getBlockByIndex(size_t index) -> Block;
    void dumpBlocks();
    void dumpKeys();
    void saveChunk(size_t chunkIndex);
    void loadChunk(size_t chunkIndex);
    void freeChunk(size_t chunkIndex);
    void saveKeys();
    void loadKeys();
    bool isValidChain(const std::vector<Block> &blocks);
    void replaceChain(const std::vector<Block> &candidateBlocks);
    uint32_t calculateNewDifficulty();
    uint32_t getDifficultyForHeight(size_t height);
    const ConsensusConfig &getConfig() const { return config; }
    uint32_t getCurrentDifficulty() const { return currentDifficulty; }
    size_t getChainBlockCount() const;
};