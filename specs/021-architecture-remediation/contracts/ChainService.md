# Contract: ChainService (Network-Domain Mediator)

**Source**: `src/ChainService.hpp` / `src/ChainService.cpp`  
**Consumers**: `PeerClient`, `PeerServer`, `BlockPropagation`

## Methods

```
// Submit a single received block (validation + append + persist)
void submitBlock(const Block &block)

// Submit a batch of blocks from sync (overlap check + append new + persist)
void submitSyncBatch(const std::vector<Block> &blocks, size_t local_height)

// Read-through convenience (delegates to IChainReader)
size_t getChainHeight() const
Block getBlockAtTip() const
const ConsensusConfig& getConsensusConfig() const
```

## Error Contract

- `submitBlock()`: Throws `ValidationError` if block is invalid (fails `isValidNewBlock()`). Throws `PersistenceError` if chunk save or key save fails.
- `submitSyncBatch()`: Throws `ValidationError` if overlap hash mismatch detected (fork). Throws `PersistenceError` on save failure.
- Read-through methods: Same contract as `IChainReader`.

## Behavioral Contract

1. **submitBlock(block)**:
   - Calls `isValidNewBlock(block, tipBlock, config)` statically
   - Calls `bc.appendBlock(block)`
   - Computes `chunk_idx = block.index / bc.chunkSize`
   - Calls `bc.saveChunk(chunk_idx)` then `bc.saveKeys()`
   - If any step throws, exception propagates to caller (network layer handles it)

2. **submitSyncBatch(blocks, local_height)**:
   - For blocks with `index < local_height`: verify hash matches local via `bc.getBlockByIndex()`; throw `ValidationError` on mismatch
   - For blocks with `index >= local_height`: validate and append
   - Persist affected chunks and keys after all blocks appended
   - Caller (PeerClient) does NOT need to know about chunk boundaries

## Guarantees

- No block is persisted without prior validation.
- Persistence always follows successful append (never skipped).
- Network components never call `appendBlock()`, `saveChunk()`, or `saveKeys()` directly.
