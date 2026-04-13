#pragma once
#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <memory>
#include "Block.hpp"
#include "StreamEntry.hpp"
#include "IChunk.hpp"
#include "IBlockchain.hpp"
#include "ConsensusConfig.hpp"
#include "ChainPersistence.hpp"
#include "DifficultyEngine.hpp"
#include "MerkleProofService.hpp"
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
    std::unordered_map<size_t, uint32_t> difficultyCache_;
    std::set<size_t> retainedChunks_;
    ChainPersistence<ChunkHandler> persistence_;
    DifficultyEngine difficultyEngine_;
    MerkleProofService proofService_;
  public:
    
    Blockchain(std::filesystem::path path, ConsensusConfig cfg = ConsensusConfig())
        : blockchainPath(path), config(cfg), currentDifficulty(cfg.initialDifficulty),
          persistence_(path, IBlockchain::chunkSize)
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
    uint32_t getCurrentDifficulty() const override { return currentDifficulty; }
    size_t getChainBlockCount() const;
    size_t getChainLength() const override;
    size_t getChunkCount() const override;
    void saveAllChunks();
    size_t discoverChunks();
    void recoverChain(bool fast_startup = false);
    bool validateChunk(size_t chunkIndex);
    void archiveChainFiles();
    void startPeriodicSave(boost::asio::io_context &io);
    void stopPeriodicSave();
    void setSaveIntervalSeconds(uint32_t interval) { save_interval_seconds_ = interval; }
    const std::filesystem::path& getBlockchainPath() const { return blockchainPath; }
    bool isDirty() const { return dirty_; }
    nlohmann::json getInclusionProof(size_t blockIndex, size_t entryIndex) override;
    nlohmann::json verifyInclusionProof(size_t blockIndex, const std::string &leafHash,
                                         const nlohmann::json &proofArray) override;

    // Chunk retention for multi-access operations
    void retainChunk(size_t chunkIndex) { retainedChunks_.insert(chunkIndex); }
    void releaseChunks() {
        for (auto idx : retainedChunks_) {
            if (idx + 1 < this->chain.size() && !this->chain[idx].blocks.empty()) {
                this->chain[idx].clear();
            }
        }
        retainedChunks_.clear();
    }

    // RAII guard for chunk retention
    class ChunkRetainGuard {
    public:
        explicit ChunkRetainGuard(Blockchain &bc) : bc_(bc) {}
        ~ChunkRetainGuard() { bc_.releaseChunks(); }
        ChunkRetainGuard(const ChunkRetainGuard&) = delete;
        ChunkRetainGuard& operator=(const ChunkRetainGuard&) = delete;
    private:
        Blockchain &bc_;
    };
};