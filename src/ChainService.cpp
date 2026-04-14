#include "ChainService.hpp"
#include "IBlockchain.hpp"
#include "ChainError.hpp"
#include <set>

void ChainService::submitBlock(const Block &block)
{
    size_t chain_height = bc_.getChainBlockCount();
    if (chain_height > 0) {
        Block tip = bc_.getBlockByIndex(chain_height - 1);
        if (!IBlockchain::isValidNewBlock(block, tip, bc_.getConfig())) {
            throw ValidationError("Block #" + std::to_string(block.index) + " failed validation");
        }
    }

    bc_.appendBlock(block);

    size_t chunk_idx = block.index / bc_.chunkSize;
    bc_.saveChunk(chunk_idx);
    bc_.saveKeys();
}

void ChainService::submitSyncBatch(const std::vector<Block> &blocks, size_t local_height)
{
    // Overlap verification
    for (const auto &block : blocks) {
        if (block.index < local_height) {
            Block local_block = bc_.getBlockByIndex(block.index);
            if (local_block.hash != block.hash) {
                throw ValidationError("Overlap hash mismatch at block " + std::to_string(block.index));
            }
            continue;
        }

        // Validate new blocks
        if (block.index > 0) {
            Block prev;
            if (block.index - 1 < bc_.getChainBlockCount()) {
                prev = bc_.getBlockByIndex(block.index - 1);
            } else {
                throw ValidationError("Cannot validate block " + std::to_string(block.index)
                                      + ": no previous block available");
            }
            if (!IBlockchain::isValidNewBlock(block, prev, bc_.getConfig())) {
                throw ValidationError("Block #" + std::to_string(block.index) + " failed validation");
            }
        }

        bc_.appendBlock(block);
    }

    // Persist affected chunks and keys
    std::set<size_t> affected_chunks;
    for (const auto &block : blocks) {
        if (block.index >= local_height) {
            affected_chunks.insert(block.index / bc_.chunkSize);
        }
    }
    for (size_t chunk_idx : affected_chunks) {
        bc_.saveChunk(chunk_idx);
    }
    bc_.saveKeys();
}
