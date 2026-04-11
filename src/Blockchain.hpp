#pragma once
#include <vector>
#include <map>
#include <set>
#include <memory>
#include "Block.hpp"
#include "StreamEntry.hpp"
#include "IChunk.hpp"
#include "IBlockchain.hpp"
#include "ConsensusConfig.hpp"
#include <filesystem>
#include <boost/asio.hpp>

template<typename ChunkHandler>
class Blockchain : public IBlockchain
{
  private:
    std::vector<ChunkHandler> chain;
    std::map<std::string, std::vector<size_t>> keyIndexMap;
    std::set<std::string> streamRegistry;
    std::map<std::string, std::map<std::string, std::vector<size_t>>> streamKeyIndex;
    std::filesystem::path blockchainPath;
    ConsensusConfig config;
    uint32_t currentDifficulty;
    bool dirty_ = false;
    size_t totalBlockCount_ = 0;
    size_t chunkCount_ = 0;
    boost::asio::io_context* io_context_ = nullptr;
    std::shared_ptr<boost::asio::steady_timer> save_timer_;
    uint32_t save_interval_seconds_ = 0;
  public:
    
    Blockchain(std::filesystem::path path, ConsensusConfig cfg = ConsensusConfig())
        : blockchainPath(path), config(cfg), currentDifficulty(cfg.initialDifficulty)
    {
        this->generateGenesisBlock();
    };
    void generateGenesisBlock();
    Block publish(const std::string &stream, const std::string &key,
                  const std::string &data, const std::vector<std::string> &keys);
    void createStream(const std::string &name);
    std::set<std::string> listStreams() const;
    std::vector<std::pair<size_t, StreamEntry>> getStreamEntries(
        const std::string &stream, const std::string &key = "") const;
    std::pair<size_t, StreamEntry> getStreamEntry(
        const std::string &stream, const std::string &key) const;
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
    void saveStreams();
    void loadStreams();
    void saveStreamIndex();
    void loadStreamIndex();
    bool isValidChain(const std::vector<Block> &blocks);
    void replaceChain(const std::vector<Block> &candidateBlocks);
    uint32_t calculateNewDifficulty();
    uint32_t getDifficultyForHeight(size_t height);
    const ConsensusConfig &getConfig() const { return config; }
    uint32_t getCurrentDifficulty() const { return currentDifficulty; }
    size_t getChainBlockCount() const;
    size_t getChainLength() const override;
    size_t getChunkCount() const override;
    void saveAllChunks();
    size_t discoverChunks();
    void recoverChain();
    bool validateChunk(size_t chunkIndex);
    void archiveChainFiles();
    void startPeriodicSave(boost::asio::io_context &io);
    void stopPeriodicSave();
    void setSaveIntervalSeconds(uint32_t interval) { save_interval_seconds_ = interval; }
    const std::filesystem::path& getBlockchainPath() const { return blockchainPath; }
    bool isDirty() const { return dirty_; }
};