#pragma once

#include "Block.hpp"
#include <functional>
#include <string>
#include <vector>

class IChainWriter {
public:
    virtual ~IChainWriter() = default;
    virtual void generateGenesisBlock() = 0;
    virtual Block publish(const std::string &stream, const std::string &key,
                          const std::string &data, const std::vector<std::string> &keys) = 0;
    virtual void createStream(const std::string &name) = 0;
    virtual void appendBlock(const Block &block) = 0;
    virtual void replaceChain(const std::vector<Block> &candidateBlocks) = 0;
    virtual void replaceChainStreaming(size_t candidateLength,
                                       std::function<std::vector<Block>(size_t batchStart, size_t batchSize)> fetcher) = 0;
    virtual void setShuttingDown() = 0;
    virtual void saveKeys() = 0;
};
