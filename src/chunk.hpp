#pragma once
#include <vector>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <boost/serialization/access.hpp>
#include <boost/serialization/vector.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>
#include "block.hpp"

class chunk
{
  public:
    std::vector<block> blocks;
    std::size_t index;
    std::filesystem::path blockchainPath;

    chunk(std::size_t index, std::filesystem::path blockchainPath) : index(index),blockchainPath(blockchainPath) {}
    void push_back(block &block) { this->blocks.push_back(block); }
    auto back() -> block & { return this->blocks.back(); }
    auto size() -> std::size_t { return this->blocks.size(); }
    auto begin() -> decltype(this->blocks.begin()) { return this->blocks.begin(); }
    auto end() -> decltype(this->blocks.end()) { return this->blocks.end(); }
    auto at(std::size_t index) -> decltype(this->blocks.at(index)) { return this->blocks.at(index); }
    auto operator[](std::size_t index) -> decltype(this->blocks[index]) { return this->blocks[index]; }
    void resize(std::size_t size) { this->blocks.resize(size); }
    void reserve(std::size_t size) { this->blocks.reserve(size); }
    void clear() { this->blocks.clear(); }
    void emplace_back(const block &block) { this->blocks.emplace_back(block); }
    bool isBlockPresent(size_t index);
    void dump();
    void save();
    void load();

    friend class boost::serialization::access;

    template<class Archive>
    void serialize(Archive &ar, const unsigned int version) {
        ar & blocks;
        ar & index;
    }
};