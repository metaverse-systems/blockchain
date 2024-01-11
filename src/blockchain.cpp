#include "blockchain.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

void blockchain::generateGenesisBlock()
{
    this->chain.emplace_back(chunk(0, this->blockchainPath));
    this->chain.at(0).emplace_back(block(0, 0, "", "GENESIS ~~DEVICE~~BLOCK"));
}

bool blockchain::isValidNewBlock(const block &newBlock, const block &previousBlock)
{
    if (previousBlock.index + 1 != newBlock.index) {
        std::cerr << "Invalid index" << std::endl;
        return false;
    } else if (previousBlock.hash != newBlock.prevHash) {
        std::cerr << "Invalid previous hash" << std::endl;
        return false;
    } else if (newBlock.calculateHash() != newBlock.hash) {
        std::cerr << "Invalid hash: " << newBlock.calculateHash() << " " << newBlock.hash << std::endl;
        return false;
    }
    return true;
}

void blockchain::addBlock(const std::string &data, const std::vector<std::string> &keys)
{
    auto unix_timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    auto currentChunk = this->chain.back();
    auto previousBlock = currentChunk.back();

    if (currentChunk.size() == this->chunkSize) {
        currentChunk = chunk(this->chain.size(), this->blockchainPath);
        this->chain.push_back(currentChunk);
    }

    auto newBlock = block(previousBlock.index + 1, unix_timestamp, previousBlock.hash, data);
    for (auto &key : keys) {
        this->keyIndexMap[key].push_back(newBlock.index);
    }
    this->chain.at(newBlock.index / this->chunkSize).push_back(newBlock);
}

auto blockchain::getBlockByIndex(size_t index) -> block
{
    size_t chunkIndex = index / this->chunkSize;

    if(this->chain.size() < chunkIndex + 1)
    {
        this->chain.resize(chunkIndex + 1, chunk(chunkIndex + 1, this->blockchainPath));
    }

    chunk chunk = this->chain.at(chunkIndex);
    
    if(!chunk.isBlockPresent(index % this->chunkSize))
    {
        this->loadChunk(chunkIndex);
    }
    
    return this->chain.at(chunkIndex).at(index % this->chunkSize);
}

std::vector<block> blockchain::getBlocksByKeys(const std::vector<std::string> &keys)
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

void blockchain::saveChunk(size_t chunkIndex)
{
    this->chain.at(chunkIndex).save();
}

void blockchain::freeChunk(size_t chunkIndex)
{
    this->chain.at(chunkIndex).clear();
}

void blockchain::loadChunk(size_t chunkIndex)
{
    this->chain.at(chunkIndex).load();
}

void blockchain::saveKeys()
{
    std::ofstream ofs(this->blockchainPath.string() + "/keys.dat", std::ios::binary);
    boost::archive::binary_oarchive oa(ofs);
    oa << this->keyIndexMap;
}

void blockchain::loadKeys()
{
    std::ifstream ifs(this->blockchainPath.string() + "/keys.dat", std::ios::binary);
    boost::archive::binary_iarchive ia(ifs);
    ia >> this->keyIndexMap;
}

void blockchain::dumpBlocks()
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

void blockchain::dumpKeys()
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