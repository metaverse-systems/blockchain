# Module Interface Contract: ChainPersistence\<ChunkHandler\>

**Module**: `ChainPersistence<ChunkHandler>`  
**Header**: `src/ChainPersistence.hpp`  
**Implementation**: `src/ChainPersistence.cpp`

## Construction

```
ChainPersistence(const std::filesystem::path& blockchainPath, size_t chunkSize)
```

Stores path and chunk size as invariant configuration. No other initialization.

## Public Methods

### Chunk I/O

```
void saveChunk(std::vector<ChunkHandler>& chain, size_t chunkIndex)
```
Serializes `chain[chunkIndex]` to `chunk_NNNNNN.dat` via Boost.Serialization.

```
void loadChunk(std::vector<ChunkHandler>& chain, size_t chunkIndex)
```
Deserializes `chunk_NNNNNN.dat` into `chain[chunkIndex]`.

```
void freeChunk(std::vector<ChunkHandler>& chain, size_t chunkIndex,
               const std::set<size_t>& retainedChunks)
```
Clears `chain[chunkIndex].blocks` if not in `retainedChunks` and not the last chunk.

### Index I/O

```
void saveKeys(const std::map<std::string, std::vector<size_t>>& keyIndexMap)
void loadKeys(std::map<std::string, std::vector<size_t>>& keyIndexMap)

void saveStreams(const std::set<std::string>& streamRegistry)
void loadStreams(std::set<std::string>& streamRegistry)

void saveStreamIndex(const StreamKeyIndex& streamKeyIndex)
void loadStreamIndex(StreamKeyIndex& streamKeyIndex)
```
Each serializes/deserializes the given data structure to/from the corresponding `.dat` file.

### Bulk Operations

```
void saveAllChunks(std::vector<ChunkHandler>& chain,
                   const std::map<std::string, std::vector<size_t>>& keyIndexMap,
                   const std::set<std::string>& streamRegistry,
                   const StreamKeyIndex& streamKeyIndex,
                   bool& dirty)
```
Saves all chunks, keys, streams, and stream index. Clears `dirty` on success.

```
size_t discoverChunks()
```
Counts existing `chunk_NNNNNN.dat` files in `blockchainPath_`. Returns count.

```
bool validateChunk(size_t chunkIndex, const ConsensusConfig& config)
```
Loads and validates a chunk's block linkage and hashes. Returns true if valid.

### Recovery & Archiving

```
void recoverChain(std::vector<ChunkHandler>& chain,
                  std::map<std::string, std::vector<size_t>>& keyIndexMap,
                  std::set<std::string>& streamRegistry,
                  StreamKeyIndex& streamKeyIndex,
                  size_t& totalBlockCount,
                  size_t& chunkCount,
                  bool& dirty,
                  std::unordered_map<size_t, uint32_t>& difficultyCache,
                  const ConsensusConfig& config,
                  bool fast_startup)
```
Discovers chunks, validates, loads indexes (or rebuilds from blocks), sets counters.

```
void archiveChainFiles(size_t chunkCount)
```
Creates timestamped backup directory and moves all chain files into it.

## Type Aliases

```
using StreamKeyIndex = std::map<std::string, std::map<std::string, std::vector<size_t>>>;
```
