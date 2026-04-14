# Contract: IChainReader (Widened Read Interface)

**Source**: `src/IChainReader.hpp`  
**Consumers**: `RpcServer`, `PeerServer`, `ChainService`, any future read-only components

## Methods

All methods are `const` (or logically read-only). No method modifies chain state.

```
// Existing methods (unchanged)
bool isShuttingDown() const
std::set<std::string> listStreams() const
std::vector<std::pair<size_t, StreamEntry>> getStreamEntries(stream, key?) const
std::pair<size_t, StreamEntry> getStreamEntry(stream, key) const
size_t getChainBlockCount() const
size_t getChainLength() const
size_t getChunkCount() const
uint32_t getCurrentDifficulty() const
const ConsensusConfig& getConfig() const

// Moved from IBlockchain (pure query, no side effects)
Block getBlockByIndex(size_t index) const
std::vector<Block> getBlocksByKeys(const std::vector<std::string> &keys) const
nlohmann::json getInclusionProof(size_t blockIndex, size_t entryIndex) const
nlohmann::json verifyInclusionProof(size_t blockIndex, const std::string &leafHash, const nlohmann::json &proofArray) const
```

## Error Contract

- `getBlockByIndex()`: Throws `ValidationError` if index >= chain length.
- `getStreamEntry()`: Throws `ValidationError` if stream or key not found.
- `getInclusionProof()`: Throws `std::out_of_range` if entry index out of range (existing behavior preserved).
- All other methods: Return empty collections or zero for absent data; never throw.

## Guarantees

- Thread safety: Same as current — callers must not interleave reads with writes without external synchronization.
- The interface does not expose `chunkSize`, `loadChunk()`, `freeChunk()`, or any persistence/lifecycle methods.
