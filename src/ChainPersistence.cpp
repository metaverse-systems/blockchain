#include "ChainPersistence.hpp"
#include "Chunk.hpp"
#include "MockChunk.hpp"
#include "utils.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/set.hpp>
#include <boost/serialization/map.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/serialization/string.hpp>

template<typename ChunkHandler>
ChainPersistence<ChunkHandler>::ChainPersistence(const std::filesystem::path& blockchainPath, size_t chunkSize)
    : blockchainPath_(blockchainPath), chunkSize_(chunkSize)
{
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::saveChunk(std::vector<ChunkHandler>& chain, size_t chunkIndex)
{
    chain.at(chunkIndex).save();
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::loadChunk(std::vector<ChunkHandler>& chain, size_t chunkIndex)
{
    chain.at(chunkIndex).load();
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::freeChunk(std::vector<ChunkHandler>& chain, size_t chunkIndex,
                                                const std::set<size_t>& retainedChunks)
{
    if (retainedChunks.count(chunkIndex)) return;
    chain.at(chunkIndex).clear();
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::saveKeys(const std::map<std::string, std::vector<size_t>>& keyIndexMap)
{
    std::ofstream ofs(blockchainPath_ / "keys.dat", std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << keyIndexMap;
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::loadKeys(std::map<std::string, std::vector<size_t>>& keyIndexMap)
{
    auto keysPath = blockchainPath_ / "keys.dat";
    if (!std::filesystem::exists(keysPath)) {
        return;
    }
    std::ifstream ifs(keysPath, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> keyIndexMap;
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::saveStreams(const std::set<std::string>& streamRegistry)
{
    std::ofstream ofs(blockchainPath_ / "streams.dat", std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << streamRegistry;
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::loadStreams(std::set<std::string>& streamRegistry)
{
    auto path = blockchainPath_ / "streams.dat";
    if (!std::filesystem::exists(path)) {
        return;
    }
    std::ifstream ifs(path, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> streamRegistry;
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::saveStreamIndex(const StreamKeyIndex& streamKeyIndex)
{
    std::ofstream ofs(blockchainPath_ / "stream_index.dat", std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << streamKeyIndex;
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::loadStreamIndex(StreamKeyIndex& streamKeyIndex)
{
    auto path = blockchainPath_ / "stream_index.dat";
    if (!std::filesystem::exists(path)) {
        return;
    }
    std::ifstream ifs(path, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> streamKeyIndex;
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::saveAllChunks(std::vector<ChunkHandler>& chain,
                                                    const std::map<std::string, std::vector<size_t>>& keyIndexMap,
                                                    const std::set<std::string>& streamRegistry,
                                                    const StreamKeyIndex& streamKeyIndex,
                                                    bool& dirty)
{
    for (size_t i = 0; i < chain.size(); i++) {
        if (chain[i].isDirty() && chain[i].size() > 0) {
            try {
                chain[i].save();
            } catch (const std::exception &e) {
                logMessage("ERROR", "Failed to save chunk " + std::to_string(i) + ": " + std::string(e.what()));
            }
        }
    }

    try {
        saveKeys(keyIndexMap);
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to save keys: " + std::string(e.what()));
    }
    try {
        saveStreams(streamRegistry);
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to save streams: " + std::string(e.what()));
    }
    try {
        saveStreamIndex(streamKeyIndex);
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to save stream index: " + std::string(e.what()));
    }

    dirty = false;
}

template<typename ChunkHandler>
size_t ChainPersistence<ChunkHandler>::discoverChunks()
{
    size_t count = 0;
    while (true) {
        if (!std::filesystem::exists(blockchainPath_ / chunkFilename(count))) {
            break;
        }
        count++;
    }
    return count;
}

template<typename ChunkHandler>
bool ChainPersistence<ChunkHandler>::validateChunk(size_t chunkIndex, const ConsensusConfig& config)
{
    (void)config;
    std::filesystem::path chunkPath = blockchainPath_ / chunkFilename(chunkIndex);

    if (std::filesystem::file_size(chunkPath) == 0) {
        logMessage("ERROR", "Chunk " + std::to_string(chunkIndex)
                   + " at " + chunkPath.string() + " is zero bytes");
        return false;
    }

    try {
        ChunkHandler chunk(chunkIndex, blockchainPath_);
        chunk.load();

        if (chunk.blocks.empty()) {
            logMessage("ERROR", "Chunk " + std::to_string(chunkIndex)
                       + " at " + chunkPath.string() + " is empty after deserialization");
            return false;
        }

        for (size_t i = 0; i < chunk.blocks.size(); i++) {
            auto &block = chunk.blocks[i];
            if (block.calculateHash() != block.hash) {
                logMessage("ERROR", "Chunk " + std::to_string(chunkIndex)
                           + " block " + std::to_string(block.index)
                           + " has invalid hash at " + chunkPath.string());
                return false;
            }
        }

        for (size_t i = 1; i < chunk.blocks.size(); i++) {
            if (chunk.blocks[i].prevHash != chunk.blocks[i - 1].hash) {
                logMessage("ERROR", "Chunk " + std::to_string(chunkIndex)
                           + " block " + std::to_string(chunk.blocks[i].index)
                           + " has broken linkage at " + chunkPath.string());
                return false;
            }
        }

        return true;
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to validate chunk " + std::to_string(chunkIndex)
                   + " at " + chunkPath.string() + ": " + std::string(e.what()));
        return false;
    }
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::recoverChain(std::vector<ChunkHandler>& chain,
                                                   std::map<std::string, std::vector<size_t>>& keyIndexMap,
                                                   std::set<std::string>& streamRegistry,
                                                   StreamKeyIndex& streamKeyIndex,
                                                   size_t& totalBlockCount,
                                                   size_t& chunkCount,
                                                   bool& dirty,
                                                   std::unordered_map<size_t, uint32_t>& difficultyCache,
                                                   const ConsensusConfig& config,
                                                   bool fast_startup)
{
    size_t numChunks = discoverChunks();

    if (numChunks == 0) {
        logMessage("INFO", "No chunk files found, starting fresh");
        return;
    }

    size_t validChunks = 0;
    size_t totalBlocks = 0;

    if (fast_startup) {
        for (size_t i = 0; i < numChunks; i++) {
            ChunkHandler chunk(i, blockchainPath_);
            chunk.load();
            totalBlocks += chunk.blocks.size();
            validChunks++;
        }
    } else {
        for (size_t i = 0; i < numChunks; i++) {
            if (!validateChunk(i, config)) {
                logMessage("WARN", "Stopping at chunk " + std::to_string(i) + " due to validation failure");
                break;
            }

            if (i > 0) {
                ChunkHandler prevChunk(i - 1, blockchainPath_);
                prevChunk.load();
                ChunkHandler currChunk(i, blockchainPath_);
                currChunk.load();

                if (!currChunk.blocks.empty() && !prevChunk.blocks.empty()) {
                    if (currChunk.blocks[0].prevHash != prevChunk.blocks.back().hash) {
                        logMessage("ERROR", "Cross-chunk linkage failed between chunk "
                                   + std::to_string(i - 1) + " and " + std::to_string(i));
                        break;
                    }
                }
            }

            ChunkHandler chunk(i, blockchainPath_);
            chunk.load();
            totalBlocks += chunk.blocks.size();
            validChunks++;
        }
    }

    if (validChunks == 0) {
        logMessage("WARN", "No valid chunks found, starting fresh");
        return;
    }

    chain.clear();
    keyIndexMap.clear();
    streamRegistry.clear();
    streamKeyIndex.clear();

    for (size_t i = 0; i < validChunks; i++) {
        chain.emplace_back(ChunkHandler(i, blockchainPath_));
    }

    chain.back().load();

    bool keysExist = std::filesystem::exists(blockchainPath_ / "keys.dat");
    bool streamsExist = std::filesystem::exists(blockchainPath_ / "streams.dat");
    bool streamIndexExist = std::filesystem::exists(blockchainPath_ / "stream_index.dat");

    if (keysExist && streamsExist && streamIndexExist) {
        try {
            loadKeys(keyIndexMap);
        } catch (...) {
            keysExist = false;
        }
        try {
            loadStreams(streamRegistry);
        } catch (...) {
            streamsExist = false;
        }
        try {
            loadStreamIndex(streamKeyIndex);
        } catch (...) {
            streamIndexExist = false;
        }
    }

    if (!keysExist || !streamsExist || !streamIndexExist) {
        logMessage("INFO", "Rebuilding missing indexes from chunk files");
        keyIndexMap.clear();
        streamRegistry.clear();
        streamKeyIndex.clear();

        for (size_t i = 0; i < validChunks; i++) {
            ChunkHandler chunk(i, blockchainPath_);
            chunk.load();
            for (const auto &block : chunk.blocks) {
                for (const auto &entry : block.entries) {
                    streamRegistry.insert(entry.stream);
                    streamKeyIndex[entry.stream][entry.key].push_back(block.index);
                }
            }
        }
    }

    totalBlockCount = totalBlocks;
    chunkCount = validChunks;
    dirty = false;
    difficultyCache.clear();

    for (size_t i = 0; i + 1 < chain.size(); i++) {
        chain[i].clear();
    }

    logMessage("INFO", "Recovered " + std::to_string(totalBlocks) + " blocks across "
               + std::to_string(validChunks) + " chunks");
}

template<typename ChunkHandler>
void ChainPersistence<ChunkHandler>::archiveChainFiles(size_t chunkCount)
{
    (void)chunkCount;
    auto backupsDir = blockchainPath_ / "backups";

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
#ifdef _WIN32
    gmtime_s(&tm_buf, &time_t_now);
#else
    gmtime_r(&time_t_now, &tm_buf);
#endif

    std::ostringstream ts;
    ts << std::put_time(&tm_buf, "%Y-%m-%dT%H%M%SZ");
    auto archiveDir = backupsDir / ts.str();

    try {
        std::filesystem::create_directories(archiveDir);
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to create backup directory: " + std::string(e.what()));
        return;
    }

    for (size_t i = 0; ; i++) {
        auto fname = chunkFilename(i);
        auto src = blockchainPath_ / fname;
        if (!std::filesystem::exists(src)) break;
        try {
            std::filesystem::rename(src, archiveDir / fname);
        } catch (const std::exception &e) {
            logMessage("ERROR", "Failed to archive " + src.string() + ": " + std::string(e.what()));
        }
    }

    for (const auto &name : {"keys.dat", "streams.dat", "stream_index.dat"}) {
        auto src = blockchainPath_ / name;
        if (std::filesystem::exists(src)) {
            try {
                std::filesystem::rename(src, archiveDir / name);
            } catch (const std::exception &e) {
                logMessage("ERROR", "Failed to archive " + src.string() + ": " + std::string(e.what()));
            }
        }
    }

    logMessage("INFO", "Archived chain files to " + archiveDir.string());
}

template class ChainPersistence<Chunk>;
template class ChainPersistence<MockChunk>;
