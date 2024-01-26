#include "blockchain.hpp"
#include "chunk.hpp"
#include "MockChunk.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

template<typename ChunkHandler>
void blockchain<ChunkHandler>::generateGenesisBlock()
{
    this->chain.emplace_back(ChunkHandler(0, this->blockchainPath));
    this->chain.at(0).emplace_back(block(0, 0, "", "GENESIS ~~DEVICE~~BLOCK"));
}

template<typename ChunkHandler>
block blockchain<ChunkHandler>::addBlock(const std::string &data, const std::vector<std::string> &keys)
{
    auto unix_timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    auto currentChunk = this->chain.back();
    auto previousBlock = currentChunk.back();

    if (currentChunk.size() == this->chunkSize) {
        currentChunk = ChunkHandler(this->chain.size(), this->blockchainPath);
        this->chain.push_back(currentChunk);
    }

    auto newBlock = block(previousBlock.index + 1, unix_timestamp, previousBlock.hash, data);
    for (auto &key : keys) {
        this->keyIndexMap[key].push_back(newBlock.index);
    }
    this->chain.at(newBlock.index / this->chunkSize).push_back(newBlock);
    return newBlock;
}

template<typename ChunkHandler>
auto blockchain<ChunkHandler>::getBlockByIndex(size_t index) -> block
{
    size_t chunkIndex = index / this->chunkSize;

    if(this->chain.size() < chunkIndex + 1)
    {
        this->chain.resize(chunkIndex + 1, ChunkHandler(chunkIndex + 1, this->blockchainPath));
    }

    ChunkHandler chunk = this->chain.at(chunkIndex);
    
    if(!chunk.isBlockPresent(index % this->chunkSize))
    {
        this->loadChunk(chunkIndex);
    }
    
    return this->chain.at(chunkIndex).at(index % this->chunkSize);
}

template<typename ChunkHandler>
std::vector<block> blockchain<ChunkHandler>::getBlocksByKeys(const std::vector<std::string> &keys)
{
    std::vector<block> blocks;
    for(auto &key : keys)
    {
        for(auto &index : this->keyIndexMap[key])
        {
            blocks.push_back(this->getBlockByIndex(index));
        }
    }
    return blocks;
}

template<typename ChunkHandler>
void blockchain<ChunkHandler>::saveChunk(size_t chunkIndex)
{
    this->chain.at(chunkIndex).save();
}

template<typename ChunkHandler>
void blockchain<ChunkHandler>::freeChunk(size_t chunkIndex)
{
    this->chain.at(chunkIndex).clear();
}

template<typename ChunkHandler>
void blockchain<ChunkHandler>::loadChunk(size_t chunkIndex)
{
    this->chain.at(chunkIndex).load();
}

template<typename ChunkHandler>
void blockchain<ChunkHandler>::saveKeys()
{
    std::ofstream ofs(this->blockchainPath.string() + "/keys.dat", std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << this->keyIndexMap;
}

template<typename ChunkHandler>
void blockchain<ChunkHandler>::loadKeys()
{
    std::ifstream ifs(this->blockchainPath.string() + "/keys.dat", std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> this->keyIndexMap;
}

template<typename ChunkHandler>
void blockchain<ChunkHandler>::dumpBlocks()
{
    for (size_t index = 0; index < this->chain.size(); index++)
    {
        std::cout << "Chain index " << index << std::endl;
        for(auto &block : this->chain[index])
        {
            block.dump();
        }
    }
}

template<typename ChunkHandler>
void blockchain<ChunkHandler>::dumpKeys()
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

template class blockchain<chunk>;
template class blockchain<MockChunk>;