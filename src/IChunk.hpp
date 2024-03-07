#pragma once

#include <iostream>
#include <filesystem>
#include <vector>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "Block.hpp"

class IChunk
{
  public:
    std::vector<Block> blocks;
    std::size_t index;
    std::filesystem::path blockchainPath;

    IChunk(std::size_t index, std::filesystem::path blockchainPath) : index(index),blockchainPath(blockchainPath) {}
    void push_back(Block &Block) { this->blocks.push_back(Block); }
    auto back() -> Block & { return this->blocks.back(); }
    auto size() -> std::size_t { return this->blocks.size(); }
    auto begin() -> decltype(this->blocks.begin()) { return this->blocks.begin(); }
    auto end() -> decltype(this->blocks.end()) { return this->blocks.end(); }
    auto at(std::size_t index) -> decltype(this->blocks.at(index)) { return this->blocks.at(index); }
    auto operator[](std::size_t index) -> decltype(this->blocks[index]) { return this->blocks[index]; }
    void resize(std::size_t size) { this->blocks.resize(size); }
    void reserve(std::size_t size) { this->blocks.reserve(size); }
    void clear() { this->blocks.clear(); }
    void emplace_back(const Block &Block) { this->blocks.emplace_back(Block); }
    bool isBlockPresent(size_t index) { return (this->blocks.size() > 0) && (this->blocks.at(index % this->blocks.size()).index == index);};
    void dump()
    {
        std::cout << "Chunk " << this->index << " has " << this->blocks.size() << " blocks" << std::endl;
        for(auto &Block : this->blocks)
        {
            Block.dump();
        }
    }
    void virtual save() = 0;
    void virtual load() = 0;

    friend class boost::serialization::access;

    template<class Archive>
    unsigned int serialize(Archive &ar, const unsigned int version) {
        ar & blocks;
        ar & index;
        return version;
    }
};