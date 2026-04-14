#pragma once

#include "Block.hpp"
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
    virtual void setShuttingDown() = 0;
    virtual void saveKeys() = 0;
};
