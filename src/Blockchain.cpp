#include "Blockchain.hpp"
#include "Chunk.hpp"
#include "MockChunk.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <stdexcept>
#include <algorithm>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include <boost/serialization/set.hpp>

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::generateGenesisBlock()
{
    this->chain.emplace_back(ChunkHandler(0, this->blockchainPath));
    this->chain.at(0).emplace_back(Block(0, 0, "", {}, 0, 0));
    this->totalBlockCount_ = 1;
    this->chunkCount_ = 1;
}

template<typename ChunkHandler>
Block Blockchain<ChunkHandler>::publish(const std::string &stream, const std::string &key,
                                         const std::string &data, const std::vector<std::string> &keys)
{
    // Auto-create stream if needed
    if (this->streamRegistry.find(stream) == this->streamRegistry.end()) {
        this->streamRegistry.insert(stream);
    }

    StreamEntry entry;
    entry.stream = stream;
    entry.key = key;
    entry.data = data;

    std::vector<StreamEntry> entries;
    entries.push_back(entry);

    auto unix_timestamp = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()));

    auto& currentChunk = this->chain.back();
    auto previousBlock = currentChunk.back();

    if (currentChunk.size() == this->chunkSize) {
        // Auto-save the filled chunk
        try {
            currentChunk.save();
        } catch (const std::exception &e) {
            logMessage("ERROR", "Failed to save filled chunk: " + std::string(e.what()));
        }
        // Free the filled chunk from memory only if it was persisted to disk
        size_t filledIndex = this->chain.size() - 1;
        std::stringstream chkSS;
        chkSS << "chunk_" << std::setfill('0') << std::setw(6) << filledIndex << ".dat";
        if (std::filesystem::exists(this->blockchainPath / chkSS.str())) {
            this->chain[filledIndex].clear();
        }
        // Create new chunk
        this->chain.emplace_back(ChunkHandler(this->chain.size(), this->blockchainPath));
        this->chunkCount_ = this->chain.size();
        this->dirty_ = false;
    }

    Block candidate;
    candidate.index = previousBlock.index + 1;
    candidate.timestamp = unix_timestamp;
    candidate.entries = entries;
    candidate.prevHash = previousBlock.hash;
    candidate.difficulty = this->currentDifficulty;
    candidate.nonce = 0;

    // Mining loop
    auto startTime = std::chrono::steady_clock::now();
    auto timeoutDuration = std::chrono::seconds(this->config.miningTimeout);

    while (true) {
        candidate.hash = candidate.calculateHash();
        if (checkLeadingZeroBits(candidate.hash, candidate.difficulty)) {
            break;
        }
        candidate.nonce++;

        auto elapsed = std::chrono::steady_clock::now() - startTime;
        if (elapsed >= timeoutDuration) {
            throw std::runtime_error("Mining timeout exceeded ("
                + std::to_string(this->config.miningTimeout) + "s)");
        }
    }

    for (auto &k : keys) {
        this->keyIndexMap[k].push_back(candidate.index);
    }

    // Update stream index
    this->streamKeyIndex[stream][key].push_back(candidate.index);

    this->chain.at(candidate.index / this->chunkSize).push_back(candidate);
    this->totalBlockCount_++;
    this->dirty_ = true;

    // Check if difficulty adjustment is needed
    size_t totalBlocks = this->getChainBlockCount();
    if (totalBlocks > 1 && (totalBlocks - 1) % this->config.adjustmentWindow == 0) {
        this->calculateNewDifficulty();
    }

    return candidate;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::createStream(const std::string &name)
{
    if (this->streamRegistry.find(name) != this->streamRegistry.end()) {
        throw std::runtime_error("Stream already exists: " + name);
    }
    this->streamRegistry.insert(name);
}

template<typename ChunkHandler>
std::set<std::string> Blockchain<ChunkHandler>::listStreams() const
{
    return this->streamRegistry;
}

template<typename ChunkHandler>
std::vector<std::pair<size_t, StreamEntry>> Blockchain<ChunkHandler>::getStreamEntries(
    const std::string &stream, const std::string &key) const
{
    std::vector<std::pair<size_t, StreamEntry>> results;
    auto streamIt = this->streamKeyIndex.find(stream);
    if (streamIt == this->streamKeyIndex.end()) {
        return results;
    }

    if (!key.empty()) {
        auto keyIt = streamIt->second.find(key);
        if (keyIt == streamIt->second.end()) {
            return results;
        }
        for (auto blockIdx : keyIt->second) {
            size_t chunkIdx = blockIdx / this->chunkSize;
            if (chunkIdx < this->chain.size()) {
                const auto &chunk = this->chain[chunkIdx];
                size_t offsetInChunk = blockIdx % this->chunkSize;
                if (offsetInChunk < chunk.blocks.size()) {
                    const Block &blk = chunk.blocks[offsetInChunk];
                    for (const auto &e : blk.entries) {
                        if (e.stream == stream && e.key == key) {
                            results.emplace_back(blockIdx, e);
                        }
                    }
                }
            }
        }
    } else {
        for (const auto &[k, indices] : streamIt->second) {
            for (auto blockIdx : indices) {
                size_t chunkIdx = blockIdx / this->chunkSize;
                if (chunkIdx < this->chain.size()) {
                    const auto &chunk = this->chain[chunkIdx];
                    size_t offsetInChunk = blockIdx % this->chunkSize;
                    if (offsetInChunk < chunk.blocks.size()) {
                        const Block &blk = chunk.blocks[offsetInChunk];
                        for (const auto &e : blk.entries) {
                            if (e.stream == stream) {
                                results.emplace_back(blockIdx, e);
                            }
                        }
                    }
                }
            }
        }
        std::sort(results.begin(), results.end(),
                  [](const auto &a, const auto &b) { return a.first < b.first; });
    }
    return results;
}

template<typename ChunkHandler>
std::pair<size_t, StreamEntry> Blockchain<ChunkHandler>::getStreamEntry(
    const std::string &stream, const std::string &key) const
{
    auto streamIt = this->streamKeyIndex.find(stream);
    if (streamIt == this->streamKeyIndex.end()) {
        throw std::runtime_error("Entry not found");
    }
    auto keyIt = streamIt->second.find(key);
    if (keyIt == streamIt->second.end() || keyIt->second.empty()) {
        throw std::runtime_error("Entry not found");
    }
    size_t lastBlockIdx = keyIt->second.back();
    size_t chunkIdx = lastBlockIdx / this->chunkSize;
    const auto &chunk = this->chain[chunkIdx];
    const Block &blk = chunk.blocks[lastBlockIdx % this->chunkSize];
    for (auto it = blk.entries.rbegin(); it != blk.entries.rend(); ++it) {
        if (it->stream == stream && it->key == key) {
            return {lastBlockIdx, *it};
        }
    }
    throw std::runtime_error("Entry not found");
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::appendBlock(const Block &block)
{
    size_t chunkIndex = block.index / this->chunkSize;

    // If the current active chunk is full, auto-save and free it
    if (!this->chain.empty() && this->chain.back().size() == this->chunkSize) {
        try {
            this->chain.back().save();
        } catch (const std::exception &e) {
            logMessage("ERROR", "Failed to save filled chunk: " + std::string(e.what()));
        }
        size_t filledIndex = this->chain.size() - 1;
        std::stringstream chkSS;
        chkSS << "chunk_" << std::setfill('0') << std::setw(6) << filledIndex << ".dat";
        if (std::filesystem::exists(this->blockchainPath / chkSS.str())) {
            this->chain[filledIndex].clear();
        }
    }

    while (this->chain.size() <= chunkIndex) {
        this->chain.emplace_back(ChunkHandler(this->chain.size(), this->blockchainPath));
    }

    Block b = block;
    this->chain[chunkIndex].push_back(b);
    this->totalBlockCount_++;
    this->chunkCount_ = this->chain.size();
    this->dirty_ = true;

    // Update stream index and registry from block entries
    for (const auto &entry : block.entries) {
        this->streamRegistry.insert(entry.stream);
        this->streamKeyIndex[entry.stream][entry.key].push_back(block.index);
    }

    // Check if difficulty adjustment is needed
    size_t totalBlocks = this->getChainBlockCount();
    if (totalBlocks > 1 && (totalBlocks - 1) % this->config.adjustmentWindow == 0) {
        this->calculateNewDifficulty();
    }
}

template<typename ChunkHandler>
auto Blockchain<ChunkHandler>::getBlockByIndex(size_t index) -> Block
{
    size_t chunkIndex = index / this->chunkSize;

    if(this->chain.size() < chunkIndex + 1)
    {
        this->chain.resize(chunkIndex + 1, ChunkHandler(chunkIndex + 1, this->blockchainPath));
    }

    bool wasEmpty = this->chain.at(chunkIndex).blocks.empty();

    if(wasEmpty || !this->chain.at(chunkIndex).isBlockPresent(index % this->chunkSize))
    {
        this->loadChunk(chunkIndex);
    }
    
    Block result = this->chain.at(chunkIndex).at(index % this->chunkSize);

    // Free filled chunks after use (not the active chunk)
    if (wasEmpty && chunkIndex + 1 < this->chain.size()) {
        this->freeChunk(chunkIndex);
    }

    return result;
}

template<typename ChunkHandler>
std::vector<Block> Blockchain<ChunkHandler>::getBlocksByKeys(const std::vector<std::string> &keys)
{
    // Collect all block indices from all keys
    std::map<size_t, std::vector<size_t>> chunkToIndices;
    for (auto &key : keys)
    {
        for (auto &index : this->keyIndexMap[key])
        {
            size_t chunkIndex = index / this->chunkSize;
            chunkToIndices[chunkIndex].push_back(index);
        }
    }

    // Load each chunk at most once and extract matching blocks
    std::vector<Block> blocks;
    for (auto &[chunkIndex, indices] : chunkToIndices)
    {
        if (this->chain.size() < chunkIndex + 1)
        {
            this->chain.resize(chunkIndex + 1, ChunkHandler(chunkIndex + 1, this->blockchainPath));
        }

        bool wasEmpty = this->chain.at(chunkIndex).blocks.empty();
        if (wasEmpty)
        {
            this->loadChunk(chunkIndex);
        }

        for (auto index : indices)
        {
            blocks.push_back(this->chain.at(chunkIndex).at(index % this->chunkSize));
        }

        // Free filled chunks after use (not the active chunk)
        if (wasEmpty && chunkIndex + 1 < this->chain.size()) {
            this->freeChunk(chunkIndex);
        }
    }
    return blocks;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::saveChunk(size_t chunkIndex)
{
    this->chain.at(chunkIndex).save();
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::freeChunk(size_t chunkIndex)
{
    this->chain.at(chunkIndex).clear();
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::loadChunk(size_t chunkIndex)
{
    this->chain.at(chunkIndex).load();
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::saveKeys()
{
    std::ofstream ofs(this->blockchainPath / "keys.dat", std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << this->keyIndexMap;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::loadKeys()
{
    auto keysPath = this->blockchainPath / "keys.dat";
    if (!std::filesystem::exists(keysPath)) {
        return;
    }
    std::ifstream ifs(keysPath, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> this->keyIndexMap;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::saveStreams()
{
    std::ofstream ofs(this->blockchainPath / "streams.dat", std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << this->streamRegistry;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::loadStreams()
{
    auto path = this->blockchainPath / "streams.dat";
    if (!std::filesystem::exists(path)) {
        return;
    }
    std::ifstream ifs(path, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> this->streamRegistry;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::saveStreamIndex()
{
    std::ofstream ofs(this->blockchainPath / "stream_index.dat", std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << this->streamKeyIndex;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::loadStreamIndex()
{
    auto path = this->blockchainPath / "stream_index.dat";
    if (!std::filesystem::exists(path)) {
        return;
    }
    std::ifstream ifs(path, std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> this->streamKeyIndex;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::dumpBlocks()
{
    for (size_t index = 0; index < this->chain.size(); index++)
    {
        std::cout << "Chain index " << index << std::endl;
        for(auto &Block : this->chain[index])
        {
            Block.dump();
        }
    }
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::dumpKeys()
{
    for(auto &key : this->keyIndexMap)
    {
        std::cout << key.first << ": ";
        for (auto &index : key.second)
        {
            std::cout << index << " ";
        }
        std::cout << std::endl;
    }
}

template<typename ChunkHandler>
size_t Blockchain<ChunkHandler>::getChainBlockCount() const
{
    size_t count = 0;
    for (const auto &chunk : this->chain) {
        count += chunk.blocks.size();
    }
    return count;
}

template<typename ChunkHandler>
size_t Blockchain<ChunkHandler>::getChainLength() const
{
    return this->totalBlockCount_;
}

template<typename ChunkHandler>
size_t Blockchain<ChunkHandler>::getChunkCount() const
{
    return this->chunkCount_;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::saveAllChunks()
{
    // Save the active (last) chunk
    if (!this->chain.empty()) {
        try {
            this->chain.back().save();
        } catch (const std::exception &e) {
            logMessage("ERROR", "Failed to save active chunk: " + std::string(e.what()));
        }
    }

    // Save all index files
    try {
        this->saveKeys();
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to save keys: " + std::string(e.what()));
    }
    try {
        this->saveStreams();
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to save streams: " + std::string(e.what()));
    }
    try {
        this->saveStreamIndex();
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to save stream index: " + std::string(e.what()));
    }

    this->dirty_ = false;
}

template<typename ChunkHandler>
size_t Blockchain<ChunkHandler>::discoverChunks()
{
    size_t count = 0;
    while (true) {
        std::stringstream ss;
        ss << "chunk_" << std::setfill('0') << std::setw(6) << count << ".dat";
        if (!std::filesystem::exists(this->blockchainPath / ss.str())) {
            break;
        }
        count++;
    }
    return count;
}

template<typename ChunkHandler>
bool Blockchain<ChunkHandler>::validateChunk(size_t chunkIndex)
{
    std::stringstream ss;
    ss << "chunk_" << std::setfill('0') << std::setw(6) << chunkIndex << ".dat";
    std::filesystem::path chunkPath = this->blockchainPath / ss.str();

    try {
        ChunkHandler chunk(chunkIndex, this->blockchainPath);
        chunk.load();

        if (chunk.blocks.empty()) {
            logMessage("ERROR", "Chunk " + std::to_string(chunkIndex)
                       + " at " + chunkPath.string() + " is empty after deserialization");
            return false;
        }

        // Verify block hashes
        for (size_t i = 0; i < chunk.blocks.size(); i++) {
            auto &block = chunk.blocks[i];
            if (block.calculateHash() != block.hash) {
                logMessage("ERROR", "Chunk " + std::to_string(chunkIndex)
                           + " block " + std::to_string(block.index)
                           + " has invalid hash at " + chunkPath.string());
                return false;
            }
        }

        // Verify internal block linkage
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
void Blockchain<ChunkHandler>::recoverChain()
{
    size_t numChunks = this->discoverChunks();

    if (numChunks == 0) {
        // No chunks on disk — fresh start with genesis already created by constructor
        logMessage("INFO", "No chunk files found, starting fresh");
        return;
    }

    // Validate contiguous prefix
    size_t validChunks = 0;
    size_t totalBlocks = 0;
    std::string lastBlockHash;

    for (size_t i = 0; i < numChunks; i++) {
        if (!this->validateChunk(i)) {
            logMessage("WARN", "Stopping at chunk " + std::to_string(i) + " due to validation failure");
            break;
        }

        // Cross-chunk linkage validation
        if (i > 0) {
            ChunkHandler prevChunk(i - 1, this->blockchainPath);
            prevChunk.load();
            ChunkHandler currChunk(i, this->blockchainPath);
            currChunk.load();

            if (!currChunk.blocks.empty() && !prevChunk.blocks.empty()) {
                if (currChunk.blocks[0].prevHash != prevChunk.blocks.back().hash) {
                    logMessage("ERROR", "Cross-chunk linkage failed between chunk "
                               + std::to_string(i - 1) + " and " + std::to_string(i));
                    break;
                }
            }
        }

        // Count blocks in this chunk
        ChunkHandler chunk(i, this->blockchainPath);
        chunk.load();
        totalBlocks += chunk.blocks.size();
        validChunks++;
    }

    if (validChunks == 0) {
        logMessage("WARN", "No valid chunks found, starting fresh");
        return;
    }

    // Clear the constructor-created genesis chain
    this->chain.clear();
    this->keyIndexMap.clear();
    this->streamRegistry.clear();
    this->streamKeyIndex.clear();

    // Create placeholders for all chunks
    for (size_t i = 0; i < validChunks; i++) {
        this->chain.emplace_back(ChunkHandler(i, this->blockchainPath));
    }

    // Load only the active (last) chunk into memory
    this->chain.back().load();

    // Rebuild indexes from all chunks
    bool keysLoaded = false, streamsLoaded = false, streamIndexLoaded = false;

    try {
        this->loadKeys();
        keysLoaded = true;
    } catch (...) {}
    try {
        this->loadStreams();
        streamsLoaded = true;
    } catch (...) {}
    try {
        this->loadStreamIndex();
        streamIndexLoaded = true;
    } catch (...) {}

    // If any index failed to load, rebuild from chunks
    if (!keysLoaded || !streamsLoaded || !streamIndexLoaded) {
        logMessage("INFO", "Rebuilding missing indexes from chunk files");
        this->keyIndexMap.clear();
        this->streamRegistry.clear();
        this->streamKeyIndex.clear();

        for (size_t i = 0; i < validChunks; i++) {
            ChunkHandler chunk(i, this->blockchainPath);
            chunk.load();
            for (const auto &block : chunk.blocks) {
                for (const auto &entry : block.entries) {
                    this->streamRegistry.insert(entry.stream);
                    this->streamKeyIndex[entry.stream][entry.key].push_back(block.index);
                }
            }
        }
    }

    this->totalBlockCount_ = totalBlocks;
    this->chunkCount_ = validChunks;
    this->dirty_ = false;

    // Free all chunks except the active one
    for (size_t i = 0; i + 1 < this->chain.size(); i++) {
        this->chain[i].clear();
    }

    logMessage("INFO", "Recovered " + std::to_string(totalBlocks) + " blocks across "
               + std::to_string(validChunks) + " chunks");
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::archiveChainFiles()
{
    auto backupsDir = this->blockchainPath / "backups";

    // Generate timestamp for backup directory
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf;
    gmtime_r(&time_t_now, &tm_buf);

    std::ostringstream ts;
    ts << std::put_time(&tm_buf, "%Y-%m-%dT%H%M%SZ");
    auto archiveDir = backupsDir / ts.str();

    try {
        std::filesystem::create_directories(archiveDir);
    } catch (const std::exception &e) {
        logMessage("ERROR", "Failed to create backup directory: " + std::string(e.what()));
        return;
    }

    // Move all chunk files
    for (size_t i = 0; ; i++) {
        std::stringstream ss;
        ss << "chunk_" << std::setfill('0') << std::setw(6) << i << ".dat";
        auto src = this->blockchainPath / ss.str();
        if (!std::filesystem::exists(src)) break;
        try {
            std::filesystem::rename(src, archiveDir / ss.str());
        } catch (const std::exception &e) {
            logMessage("ERROR", "Failed to archive " + src.string() + ": " + std::string(e.what()));
        }
    }

    // Move index files
    for (const auto &name : {"keys.dat", "streams.dat", "stream_index.dat"}) {
        auto src = this->blockchainPath / name;
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

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::startPeriodicSave(boost::asio::io_context &io)
{
    this->io_context_ = &io;

    if (this->save_interval_seconds_ == 0) {
        return;
    }

    this->save_timer_ = std::make_shared<boost::asio::steady_timer>(io);
    auto self = this;

    std::function<void(const boost::system::error_code&)> handler;
    handler = [self, &handler](const boost::system::error_code &ec) {
        if (ec) return;

        if (self->dirty_) {
            try {
                self->saveAllChunks();
            } catch (const std::exception &e) {
                logMessage("ERROR", "Periodic save failed: " + std::string(e.what()));
            }
        }

        // Reschedule
        if (self->save_timer_) {
            self->save_timer_->expires_after(std::chrono::seconds(self->save_interval_seconds_));
            self->save_timer_->async_wait(handler);
        }
    };

    this->save_timer_->expires_after(std::chrono::seconds(this->save_interval_seconds_));
    this->save_timer_->async_wait(handler);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::stopPeriodicSave()
{
    if (this->save_timer_) {
        this->save_timer_->cancel();
        this->save_timer_.reset();
    }
}

template<typename ChunkHandler>
bool Blockchain<ChunkHandler>::isValidChain(const std::vector<Block> &blocks)
{
    if (blocks.empty()) return false;

    // Verify genesis block structure
    const auto &genesis = blocks[0];
    if (genesis.index != 0) return false;

    for (size_t i = 1; i < blocks.size(); i++) {
        if (!IBlockchain::isValidNewBlock(blocks[i], blocks[i - 1], this->config)) {
            return false;
        }
    }
    return true;
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::replaceChain(const std::vector<Block> &candidateBlocks)
{
    size_t currentLength = this->getChainBlockCount();

    if (candidateBlocks.size() <= currentLength) {
        logMessage("WARN", "Candidate chain is not longer than current chain");
        return;
    }

    if (candidateBlocks.size() - currentLength > this->config.maxReorgDepth) {
        logMessage("WARN", "Chain reorganization depth exceeds maximum of "
                   + std::to_string(this->config.maxReorgDepth));
        return;
    }

    if (!this->isValidChain(candidateBlocks)) {
        logMessage("WARN", "Candidate chain is not valid");
        return;
    }

    // Archive old chain files before replacing
    this->archiveChainFiles();

    // Clear current chain
    this->chain.clear();
    this->keyIndexMap.clear();
    this->streamRegistry.clear();
    this->streamKeyIndex.clear();

    // Rebuild chain from candidate blocks
    for (const auto &block : candidateBlocks) {
        size_t chunkIndex = block.index / this->chunkSize;
        while (this->chain.size() <= chunkIndex) {
            this->chain.emplace_back(ChunkHandler(this->chain.size(), this->blockchainPath));
        }
        Block b = block;
        this->chain[chunkIndex].push_back(b);

        // Rebuild stream index
        for (const auto &entry : block.entries) {
            this->streamRegistry.insert(entry.stream);
            this->streamKeyIndex[entry.stream][entry.key].push_back(block.index);
        }
    }

    this->totalBlockCount_ = candidateBlocks.size();
    this->chunkCount_ = this->chain.size();
    this->dirty_ = true;

    // Save all chunks of the new chain
    for (size_t i = 0; i < this->chain.size(); i++) {
        try {
            this->chain[i].save();
        } catch (const std::exception &e) {
            logMessage("ERROR", "Failed to save chunk " + std::to_string(i) + " after replaceChain: " + std::string(e.what()));
        }
    }
    this->saveKeys();
    this->saveStreams();
    this->saveStreamIndex();
    this->dirty_ = false;

    logMessage("INFO", "Chain replaced with candidate chain of length "
               + std::to_string(candidateBlocks.size()));
}

template<typename ChunkHandler>
uint32_t Blockchain<ChunkHandler>::calculateNewDifficulty()
{
    size_t totalBlocks = this->getChainBlockCount();
    if (totalBlocks < this->config.adjustmentWindow + 1) {
        return this->currentDifficulty;
    }

    // Get the first and last block in the current window
    size_t windowEnd = totalBlocks - 1;
    size_t windowStart = windowEnd - this->config.adjustmentWindow;

    Block firstBlock = this->getBlockByIndex(windowStart);
    Block lastBlock = this->getBlockByIndex(windowEnd);

    double expectedTime = static_cast<double>(this->config.targetBlockInterval) * this->config.adjustmentWindow;
    double actualTime = static_cast<double>(lastBlock.timestamp - firstBlock.timestamp);

    if (actualTime <= 0) actualTime = 1.0;

    double ratio = expectedTime / actualTime;

    // Clamp ratio
    double maxFactor = this->config.maxAdjustmentFactor;
    if (ratio > maxFactor) ratio = maxFactor;
    if (ratio < 1.0 / maxFactor) ratio = 1.0 / maxFactor;

    int32_t adjustment = static_cast<int32_t>(std::round(std::log2(ratio)));
    int32_t newDiff = static_cast<int32_t>(this->currentDifficulty) + adjustment;

    // Clamp to [minDifficulty, maxDifficulty]
    if (newDiff < static_cast<int32_t>(this->config.minDifficulty))
        newDiff = static_cast<int32_t>(this->config.minDifficulty);
    if (newDiff > static_cast<int32_t>(this->config.maxDifficulty))
        newDiff = static_cast<int32_t>(this->config.maxDifficulty);

    this->currentDifficulty = static_cast<uint32_t>(newDiff);
    return this->currentDifficulty;
}

template<typename ChunkHandler>
uint32_t Blockchain<ChunkHandler>::getDifficultyForHeight(size_t height)
{
    if (height == 0) return 0;

    // Walk through adjustment boundaries to compute what difficulty should be at this height
    uint32_t difficulty = this->config.initialDifficulty;

    for (size_t boundaryHeight = this->config.adjustmentWindow;
         boundaryHeight <= height;
         boundaryHeight += this->config.adjustmentWindow)
    {
        size_t windowStart = boundaryHeight - this->config.adjustmentWindow;
        size_t windowEnd = boundaryHeight;

        // Ensure blocks exist for this window
        size_t totalBlocks = this->getChainBlockCount();
        if (windowEnd >= totalBlocks) break;

        Block firstBlock = this->getBlockByIndex(windowStart);
        Block lastBlock = this->getBlockByIndex(windowEnd);

        double expectedTime = static_cast<double>(this->config.targetBlockInterval) * this->config.adjustmentWindow;
        double actualTime = static_cast<double>(lastBlock.timestamp - firstBlock.timestamp);
        if (actualTime <= 0) actualTime = 1.0;

        double ratio = expectedTime / actualTime;
        double maxFactor = this->config.maxAdjustmentFactor;
        if (ratio > maxFactor) ratio = maxFactor;
        if (ratio < 1.0 / maxFactor) ratio = 1.0 / maxFactor;

        int32_t adjustment = static_cast<int32_t>(std::round(std::log2(ratio)));
        int32_t newDiff = static_cast<int32_t>(difficulty) + adjustment;

        if (newDiff < static_cast<int32_t>(this->config.minDifficulty))
            newDiff = static_cast<int32_t>(this->config.minDifficulty);
        if (newDiff > static_cast<int32_t>(this->config.maxDifficulty))
            newDiff = static_cast<int32_t>(this->config.maxDifficulty);

        difficulty = static_cast<uint32_t>(newDiff);
    }

    return difficulty;
}

template class Blockchain<Chunk>;
template class Blockchain<MockChunk>;