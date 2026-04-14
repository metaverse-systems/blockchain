#pragma once

#include "IBlockchain.hpp"
#include "ChainError.hpp"
#include "Block.hpp"
#include "ConsensusConfig.hpp"
#include <vector>

class ChainService {
public:
    explicit ChainService(IBlockchain &bc) : bc_(bc) {}

    void submitBlock(const Block &block);
    void submitSyncBatch(const std::vector<Block> &blocks, size_t local_height);

    size_t getChainHeight() const { return bc_.getChainBlockCount(); }
    Block getBlockAtTip() { return bc_.getBlockByIndex(bc_.getChainBlockCount() - 1); }
    const ConsensusConfig& getConsensusConfig() const { return bc_.getConfig(); }

private:
    IBlockchain &bc_;
};
