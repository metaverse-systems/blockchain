#pragma once

#include <vector>
#include <map>
#include <set>
#include <unordered_map>
#include <optional>
#include <filesystem>
#include "Block.hpp"
#include "StreamEntry.hpp"
#include "ConsensusConfig.hpp"

using StreamKeyIndex = std::map<std::string, std::map<std::string, std::vector<size_t>>>;

template<typename ChunkHandler>
class ChainPersistence
{
  private:
    std::filesystem::path blockchainPath_;
    size_t chunkSize_;

  public:
    ChainPersistence(const std::filesystem::path& blockchainPath, size_t chunkSize);

    void saveChunk(std::vector<ChunkHandler>& chain, size_t chunkIndex);
    void loadChunk(std::vector<ChunkHandler>& chain, size_t chunkIndex);
    void freeChunk(std::vector<ChunkHandler>& chain, size_t chunkIndex,
                   const std::set<size_t>& retainedChunks);

    void saveKeys(const std::map<std::string, std::vector<size_t>>& keyIndexMap);
    void loadKeys(std::map<std::string, std::vector<size_t>>& keyIndexMap);

    void saveStreams(const std::set<std::string>& streamRegistry);
    void loadStreams(std::set<std::string>& streamRegistry);

    void saveStreamIndex(const StreamKeyIndex& streamKeyIndex);
    void loadStreamIndex(StreamKeyIndex& streamKeyIndex);

    size_t saveAllChunks(std::vector<ChunkHandler>& chain,
                       const std::map<std::string, std::vector<size_t>>& keyIndexMap,
                       const std::set<std::string>& streamRegistry,
                       const StreamKeyIndex& streamKeyIndex,
                       bool& dirty);

    size_t discoverChunks();
    std::optional<ChunkHandler> validateChunk(size_t chunkIndex, const ConsensusConfig& config);

    void recoverChain(std::vector<ChunkHandler>& chain,
                      std::map<std::string, std::vector<size_t>>& keyIndexMap,
                      std::set<std::string>& streamRegistry,
                      StreamKeyIndex& streamKeyIndex,
                      size_t& totalBlockCount,
                      size_t& chunkCount,
                      bool& dirty,
                      std::unordered_map<size_t, uint32_t>& difficultyCache,
                      const ConsensusConfig& config,
                      bool fast_startup);

    void archiveChainFiles(size_t chunkCount);
};
