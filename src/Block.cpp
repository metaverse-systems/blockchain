#include "Block.hpp"
#include <sstream>
#include <iostream>
#include <ctime>

Block::Block()
{
    this->index = 0;
    this->timestamp = 0;
    this->data = "";
    this->prevHash = "";
    this->hash = "";
}

Block::Block(size_t index, uint64_t time, std::string prev_hash, std::string block_data)
    : index(index), timestamp(time), data(std::move(block_data)), prevHash(std::move(prev_hash)) {
    this->hash = calculateHash();
}

std::string Block::calculateHash() const
{
    std::stringstream ss;
    ss << this->index << this->timestamp << this->data << this->prevHash;
    return sha256(ss.str());
}

void Block::dump()
{
    std::cout << "Block #" << this->index << std::endl;
    std::cout << "Hash: " << this->hash << std::endl;
    std::cout << "Previous Hash: " << this->prevHash << std::endl;
    std::cout << "Block Data: " << this->data << std::endl;
    std::cout << "Created at: " << this->timestamp << std::endl;
    // Print time in human readable format
    std::cout << "Created at (human readable): " << std::ctime((const time_t *)&this->timestamp) << std::endl;
    std::cout << std::endl;
}

nlohmann::json Block::toJson() const
{
    nlohmann::json j;
    j["index"] = this->index;
    j["timestamp"] = this->timestamp;
    j["data"] = this->data;
    j["prevHash"] = this->prevHash;
    j["hash"] = this->hash;
    return j;
}