#include "Block.hpp"
#include <sstream>
#include <iostream>
#include <ctime>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/archive/binary_iarchive.hpp>

Block::Block()
{
    this->index = 0;
    this->timestamp = 0;
    this->prevHash = "";
    this->hash = "";
    this->nonce = 0;
    this->difficulty = 0;
    this->merkleRoot = "";
}

Block::Block(size_t index, uint64_t time, std::string prev_hash, std::vector<StreamEntry> entries, uint64_t nonce, uint32_t difficulty)
    : index(index), timestamp(time), entries(std::move(entries)), prevHash(std::move(prev_hash)),
      nonce(nonce), difficulty(difficulty) {
    // Compute leaf hashes from entries
    std::vector<std::string> leafHashes;
    for (const auto &entry : this->entries) {
        std::ostringstream oss;
        boost::archive::binary_oarchive oa(oss);
        oa << entry;
        leafHashes.push_back(MerkleTree::computeLeafHash(oss.str()));
    }
    this->merkleRoot = MerkleTree::computeMerkleRoot(leafHashes);
    this->hash = calculateHash();
}

Block::Block(size_t index, uint64_t time, std::string prev_hash,
             std::vector<StreamEntry> entries, uint64_t nonce, uint32_t difficulty,
             std::string merkleRoot, std::string hash)
    : index(index), timestamp(time), entries(std::move(entries)), prevHash(std::move(prev_hash)),
      nonce(nonce), difficulty(difficulty), merkleRoot(std::move(merkleRoot)), hash(std::move(hash)) {
    // Verify merkle root against entries
    std::vector<std::string> leafHashes;
    for (const auto &entry : this->entries) {
        std::ostringstream oss;
        boost::archive::binary_oarchive oa(oss);
        oa << entry;
        leafHashes.push_back(MerkleTree::computeLeafHash(oss.str()));
    }
    std::string computedRoot = MerkleTree::computeMerkleRoot(leafHashes);
    if (computedRoot != this->merkleRoot) {
        throw std::invalid_argument("Merkle root mismatch: expected " + computedRoot
                                    + " got " + this->merkleRoot);
    }
    // Verify hash against block contents
    std::string computedHash = calculateHash();
    if (computedHash != this->hash) {
        throw std::invalid_argument("Block hash mismatch: expected " + computedHash
                                    + " got " + this->hash);
    }
}

std::string Block::calculateHash() const
{
    std::stringstream ss;
    ss << this->index << this->timestamp;
    ss << this->merkleRoot;
    ss << this->prevHash << this->nonce << this->difficulty;
    return sha256(ss.str());
}

void Block::dump()
{
    std::cout << "Block #" << this->index << std::endl;
    std::cout << "Hash: " << this->hash << std::endl;
    std::cout << "Previous Hash: " << this->prevHash << std::endl;
    std::cout << "Entries: " << this->entries.size() << std::endl;
    for (const auto &e : this->entries) {
        std::cout << "  Stream: " << e.stream << " Key: " << e.key << " Data: " << e.data << std::endl;
    }
    std::cout << "Created at: " << this->timestamp << std::endl;
    std::cout << "Created at (human readable): " << std::ctime((const time_t *)&this->timestamp) << std::endl;
    std::cout << std::endl;
}

nlohmann::json Block::toJson() const
{
    nlohmann::json j;
    j["index"] = this->index;
    j["timestamp"] = this->timestamp;
    j["prevHash"] = this->prevHash;
    j["merkleRoot"] = this->merkleRoot;
    j["hash"] = this->hash;
    j["nonce"] = this->nonce;
    j["difficulty"] = this->difficulty;
    nlohmann::json entriesJson = nlohmann::json::array();
    for (const auto &e : this->entries) {
        nlohmann::json ej;
        ej["stream"] = e.stream;
        ej["key"] = e.key;
        ej["data"] = e.data;
        entriesJson.push_back(ej);
    }
    j["entries"] = entriesJson;
    return j;
}

nlohmann::json Block::toHeaderJson() const
{
    nlohmann::json j;
    j["index"] = this->index;
    j["timestamp"] = this->timestamp;
    j["prevHash"] = this->prevHash;
    j["merkleRoot"] = this->merkleRoot;
    j["nonce"] = this->nonce;
    j["difficulty"] = this->difficulty;
    j["hash"] = this->hash;
    return j;
}