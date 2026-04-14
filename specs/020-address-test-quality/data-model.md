# Data Model: Address Test Quality

**Feature**: 020-address-test-quality  
**Date**: 2026-04-13

## Interface Entities

### IChainReader (new)

Read-only view of blockchain state. Used by consumers that only query data.

| Method | Signature | Description |
|--------|-----------|-------------|
| isShuttingDown | `bool isShuttingDown() const` | Check if node is shutting down |
| listStreams | `std::set<std::string> listStreams() const` | List all stream names |
| getStreamEntries | `std::vector<std::pair<size_t, StreamEntry>> getStreamEntries(const std::string &stream, const std::string &key) const` | Query stream entries |
| getStreamEntry | `std::pair<size_t, StreamEntry> getStreamEntry(const std::string &stream, const std::string &key) const` | Get single stream entry |
| getChainBlockCount | `size_t getChainBlockCount() const` | Total block count |
| getChainLength | `size_t getChainLength() const` | Chain length (may differ from block count) |
| getChunkCount | `size_t getChunkCount() const` | Number of chunks |
| getCurrentDifficulty | `uint32_t getCurrentDifficulty() const` | Current mining difficulty |
| getConfig | `const ConsensusConfig& getConfig() const` | Consensus configuration |
| isValidNewBlock | `static bool isValidNewBlock(const Block &, const Block &, const ConsensusConfig &)` | Block validation (static) |

### IChainWriter (new)

Mutation interface for blockchain state changes.

| Method | Signature | Description |
|--------|-----------|-------------|
| generateGenesisBlock | `void generateGenesisBlock()` | Create genesis block |
| publish | `Block publish(const std::string &stream, const std::string &key, const std::string &data, const std::vector<std::string> &keys)` | Mine and append a block |
| createStream | `void createStream(const std::string &name)` | Create a named stream |
| appendBlock | `void appendBlock(const Block &block)` | Append a validated block |
| replaceChain | `void replaceChain(const std::vector<Block> &candidateBlocks)` | Replace chain with longer candidate |
| setShuttingDown | `void setShuttingDown()` | Signal shutdown |
| saveKeys | `void saveKeys()` | Persist key index |

### IBlockchain (modified)

Inherits `IChainReader` and `IChainWriter`. Retains persistence and diagnostic methods not needed by most consumers.

**Relationship**: `IBlockchain : public IChainReader, public IChainWriter`

### MockChainReader (new, test-only)

Minimal mock implementing only `IChainReader` for RPC query handler tests. Stores configurable return values for each method.

### MockChainWriter (new, test-only)

Minimal mock implementing only `IChainWriter` for RPC mutation handler tests. Records calls for verification.

## Modified Entities

### ChainPersistence::saveAllChunks() 

**Before**: Returns `void`, sets `dirty = false` unconditionally.

**After**: Returns `size_t` (count of failed operations), sets `dirty = false` only when all operations succeed (failure count == 0).

| Field | Type | Description |
|-------|------|-------------|
| Return value | `size_t` | Number of operations that failed (0 = all succeeded) |
| dirty flag | `bool&` | Only cleared when return value == 0 |

### RpcServer (modified)

**Change**: Add `friend class RpcHandlerTests;` declaration to enable direct handler invocation in tests.

No public API changes.

## State Transitions

### saveAllChunks() dirty flag

```
  dirty=true
      │
      ▼
  saveAllChunks()
      │
      ├── all succeed (failures=0) ──► dirty=false
      │
      └── any fail (failures>0) ──► dirty=true (unchanged)
```
