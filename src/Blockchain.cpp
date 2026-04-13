#include "Blockchain.hpp"
#include "Chunk.hpp"
#include "MockChunk.hpp"
#include "MerkleTree.hpp"
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
    if (this->isShuttingDown()) {
        throw std::runtime_error("Cannot publish block: node is shutting down");
    }

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
        if (std::filesystem::exists(this->blockchainPath / chunkFilename(filledIndex))) {
            this->chain[filledIndex].clear();
        }
        // Create new chunk
        this->chain.emplace_back(ChunkHandler(this->chain.size(), this->blockchainPath));
        this->chunkCount_ = this->chain.size();
    }

    Block candidate;
    candidate.index = previousBlock.index + 1;
    candidate.timestamp = unix_timestamp;
    candidate.entries = entries;
    candidate.prevHash = previousBlock.hash;
    candidate.difficulty = this->currentDifficulty;
    candidate.nonce = 0;

    // Compute merkleRoot before mining
    std::vector<std::string> leafHashes;
    for (const auto &entry : candidate.entries) {
        std::ostringstream entryOss;
        boost::archive::binary_oarchive entryOa(entryOss);
        entryOa << entry;
        leafHashes.push_back(MerkleTree::computeLeafHash(entryOss.str()));
    }
    candidate.merkleRoot = MerkleTree::computeMerkleRoot(leafHashes);

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

    auto collectEntries = [&](size_t blockIdx, const std::string &filterKey) {
        size_t chunkIdx = blockIdx / this->chunkSize;
        if (chunkIdx < this->chain.size()) {
            const auto &chunk = this->chain[chunkIdx];
            size_t offsetInChunk = blockIdx % this->chunkSize;
            if (offsetInChunk < chunk.blocks.size()) {
                const Block &blk = chunk.blocks[offsetInChunk];
                for (const auto &e : blk.entries) {
                    if (e.stream == stream && (filterKey.empty() || e.key == filterKey)) {
                        results.emplace_back(blockIdx, e);
                    }
                }
            }
        }
    };

    if (!key.empty()) {
        auto keyIt = streamIt->second.find(key);
        if (keyIt == streamIt->second.end()) {
            return results;
        }
        for (auto blockIdx : keyIt->second) {
            collectEntries(blockIdx, key);
        }
    } else {
        for (const auto &[k, indices] : streamIt->second) {
            for (auto blockIdx : indices) {
                collectEntries(blockIdx, "");
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
    if (this->isShuttingDown()) {
        throw std::runtime_error("Cannot append block: node is shutting down");
    }

    size_t chunkIndex = block.index / this->chunkSize;

    // If the current active chunk is full, auto-save and free it
    if (!this->chain.empty() && this->chain.back().size() == this->chunkSize) {
        try {
            this->chain.back().save();
        } catch (const std::exception &e) {
            logMessage("ERROR", "Failed to save filled chunk: " + std::string(e.what()));
        }
        size_t filledIndex = this->chain.size() - 1;
        if (std::filesystem::exists(this->blockchainPath / chunkFilename(filledIndex))) {
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
    persistence_.saveChunk(this->chain, chunkIndex);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::freeChunk(size_t chunkIndex)
{
    persistence_.freeChunk(this->chain, chunkIndex, retainedChunks_);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::loadChunk(size_t chunkIndex)
{
    persistence_.loadChunk(this->chain, chunkIndex);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::saveKeys()
{
    persistence_.saveKeys(this->keyIndexMap);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::loadKeys()
{
    persistence_.loadKeys(this->keyIndexMap);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::saveStreams()
{
    persistence_.saveStreams(this->streamRegistry);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::loadStreams()
{
    persistence_.loadStreams(this->streamRegistry);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::saveStreamIndex()
{
    persistence_.saveStreamIndex(this->streamKeyIndex);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::loadStreamIndex()
{
    persistence_.loadStreamIndex(this->streamKeyIndex);
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
    return this->totalBlockCount_;
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
    persistence_.saveAllChunks(this->chain, this->keyIndexMap,
                               this->streamRegistry, this->streamKeyIndex, this->dirty_);
}

template<typename ChunkHandler>
size_t Blockchain<ChunkHandler>::discoverChunks()
{
    return persistence_.discoverChunks();
}

template<typename ChunkHandler>
bool Blockchain<ChunkHandler>::validateChunk(size_t chunkIndex)
{
    return persistence_.validateChunk(chunkIndex, this->config);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::recoverChain(bool fast_startup)
{
    persistence_.recoverChain(this->chain, this->keyIndexMap,
                               this->streamRegistry, this->streamKeyIndex,
                               this->totalBlockCount_, this->chunkCount_,
                               this->dirty_, this->difficultyCache_,
                               this->config, fast_startup);
}

template<typename ChunkHandler>
void Blockchain<ChunkHandler>::archiveChainFiles()
{
    persistence_.archiveChainFiles(this->chunkCount_);
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
    this->difficultyCache_.clear();

    logMessage("INFO", "Chain replaced with candidate chain of length "
               + std::to_string(candidateBlocks.size()));
}

template<typename ChunkHandler>
uint32_t Blockchain<ChunkHandler>::calculateNewDifficulty()
{
    auto getBlock = [this](size_t idx) -> Block { return this->getBlockByIndex(idx); };
    this->currentDifficulty = difficultyEngine_.calculateNewDifficulty(
        this->config, this->getChainBlockCount(), this->currentDifficulty, getBlock);
    return this->currentDifficulty;
}

template<typename ChunkHandler>
uint32_t Blockchain<ChunkHandler>::getDifficultyForHeight(size_t height)
{
    ChunkRetainGuard guard(*this);
    auto getBlock = [this](size_t idx) -> Block { return this->getBlockByIndex(idx); };
    auto retainChunk = [this](size_t idx) { this->retainChunk(idx / this->chunkSize); };
    return difficultyEngine_.getDifficultyForHeight(
        height, this->config, this->getChainBlockCount(),
        this->difficultyCache_, getBlock, retainChunk);
}

template<typename ChunkHandler>
nlohmann::json Blockchain<ChunkHandler>::getInclusionProof(size_t blockIndex, size_t entryIndex)
{
    Block block = this->getBlockByIndex(blockIndex);
    return proofService_.getInclusionProof(block, entryIndex);
}

template<typename ChunkHandler>
nlohmann::json Blockchain<ChunkHandler>::verifyInclusionProof(size_t blockIndex, const std::string &leafHash,
                                                               const nlohmann::json &proofArray)
{
    Block block = this->getBlockByIndex(blockIndex);
    return proofService_.verifyInclusionProof(block, leafHash, proofArray);
}

template class Blockchain<Chunk>;
template class Blockchain<MockChunk>;