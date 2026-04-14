# Interface Contract: IChainReader / IChainWriter

**Feature**: 020-address-test-quality  
**Date**: 2026-04-13

## IChainReader

Pure virtual interface for read-only blockchain access.

```cpp
#pragma once
#include "Block.hpp"
#include "StreamEntry.hpp"
#include "ConsensusConfig.hpp"
#include <set>
#include <string>
#include <vector>
#include <utility>

class IChainReader {
public:
    virtual ~IChainReader() = default;
    virtual bool isShuttingDown() const = 0;
    virtual std::set<std::string> listStreams() const = 0;
    virtual std::vector<std::pair<size_t, StreamEntry>> getStreamEntries(
        const std::string &stream, const std::string &key = "") const = 0;
    virtual std::pair<size_t, StreamEntry> getStreamEntry(
        const std::string &stream, const std::string &key) const = 0;
    virtual size_t getChainBlockCount() const = 0;
    virtual size_t getChainLength() const = 0;
    virtual size_t getChunkCount() const = 0;
    virtual uint32_t getCurrentDifficulty() const = 0;
    virtual const ConsensusConfig& getConfig() const = 0;
    static bool isValidNewBlock(const Block &newBlock, const Block &previousBlock,
                                const ConsensusConfig &config);
};
```

## IChainWriter

Pure virtual interface for blockchain state mutations.

```cpp
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
```

## IBlockchain (modified)

Inherits both sub-interfaces. Retains persistence/diagnostic methods.

```cpp
class IBlockchain : public IChainReader, public IChainWriter {
public:
    // Persistence methods (not in sub-interfaces)
    virtual void loadChunk(size_t chunk_id) = 0;
    virtual void freeChunk(size_t chunk_id) = 0;
    virtual void saveChunk(size_t chunk_id) = 0;
    virtual void loadKeys() = 0;
    virtual void dumpBlocks() = 0;
    virtual void dumpKeys() = 0;
    
    // Query methods returning mutable copies (kept on IBlockchain)
    virtual std::vector<Block> getBlocksByKeys(const std::vector<std::string> &keys) = 0;
    virtual Block getBlockByIndex(size_t index) = 0;
    
    // Merkle proof methods
    virtual nlohmann::json getInclusionProof(size_t blockIndex, size_t entryIndex) = 0;
    virtual nlohmann::json verifyInclusionProof(size_t blockIndex,
        const std::string &leafHash, const nlohmann::json &proofArray) = 0;
};
```

## saveAllChunks() Contract Change

**Before**:
```cpp
void saveAllChunks(std::vector<ChunkHandler>& chain, ..., bool& dirty);
// Always sets dirty = false, returns void
```

**After**:
```cpp
size_t saveAllChunks(std::vector<ChunkHandler>& chain, ..., bool& dirty);
// Returns count of failed operations (0 = success)
// Sets dirty = false ONLY when return value == 0
```

## RpcServer Friend Access

```cpp
class RpcServer : public SessionHandler {
    friend class RpcHandlerTests;  // NEW: enable direct handler testing
    // ... rest unchanged
};
```
