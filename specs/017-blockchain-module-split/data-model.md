# Data Model: Blockchain Module Split

**Date**: 2026-04-12  
**Feature**: 017-blockchain-module-split

## Entity: ChainPersistence\<ChunkHandler\>

**Purpose**: Manages all disk I/O for blockchain data — chunk files, key indexes, stream indexes, chain recovery, and archiving.

### Fields (owned)

| Field | Type | Description |
|-------|------|-------------|
| `blockchainPath_` | `std::filesystem::path` | Reference to blockchain data directory (received from core) |
| `chunkSize_` | `size_t` | Blocks per chunk (received from core, matches `IBlockchain::chunkSize`) |

### Fields (received by reference from core)

| Field | Type | Used By |
|-------|------|---------|
| `chain` | `std::vector<ChunkHandler>&` | save/load/free/recover/validate |
| `keyIndexMap` | `std::map<std::string, std::vector<size_t>>&` | saveKeys/loadKeys/recovery |
| `streamRegistry` | `std::set<std::string>&` | saveStreams/loadStreams/recovery |
| `streamKeyIndex` | `std::map<...>&` | saveStreamIndex/loadStreamIndex/recovery |
| `dirty_` | `bool&` | saveAllChunks (reads/clears), recovery (sets) |
| `totalBlockCount_` | `size_t&` | recovery (sets) |
| `chunkCount_` | `size_t&` | recovery (sets), archiveChainFiles |
| `retainedChunks_` | `std::set<size_t>&` | freeChunk (reads) |
| `difficultyCache_` | `std::unordered_map<size_t, uint32_t>&` | recovery (clears) |

### Relationships

- **Owned by**: Blockchain\<ChunkHandler\> (composition)
- **Depends on**: Block, StreamEntry, ConsensusConfig (value types)
- **No dependency on**: DifficultyEngine, MerkleProofService

### State Transitions

- `recoverChain()` transitions the entire chain state from "empty/corrupt" to "fully loaded and validated"
- `saveAllChunks()` transitions `dirty_` from `true` to `false`
- `archiveChainFiles()` creates timestamped backup, then chain state is rebuilt

---

## Entity: DifficultyEngine

**Purpose**: Calculates proof-of-work difficulty for new blocks and historical heights using the adjustment window algorithm.

### Fields (owned)

None. All state is passed as parameters. The difficulty cache (`difficultyCache_`) is owned by Blockchain core and passed by reference.

### Key Parameters (received per call)

| Parameter | Type | Used By |
|-----------|------|---------|
| `config` | `const ConsensusConfig&` | Both methods (adjustmentWindow, targetBlockInterval, etc.) |
| `totalBlockCount` | `size_t` | calculateNewDifficulty, getDifficultyForHeight |
| `currentDifficulty` | `uint32_t` | calculateNewDifficulty |
| `difficultyCache` | `std::unordered_map<size_t, uint32_t>&` | getDifficultyForHeight (reads/writes) |
| Block accessor | Callback `std::function<Block(size_t)>` | getDifficultyForHeight (fetches blocks by index) |
| Chunk retainer | Callback `std::function<void(size_t)>` | getDifficultyForHeight (retains chunks during scan) |

### Relationships

- **Owned by**: Blockchain\<ChunkHandler\> (composition)
- **Depends on**: Block (timestamp field), ConsensusConfig
- **No dependency on**: ChainPersistence, MerkleProofService, chunk types

### Validation Rules

- Difficulty clamped to `[config.minDifficulty, config.maxDifficulty]`
- Adjustment factor clamped to `[1/maxAdjustmentFactor, maxAdjustmentFactor]`
- Returns `config.initialDifficulty` when block count ≤ adjustment window

---

## Entity: MerkleProofService

**Purpose**: Generates and verifies Merkle inclusion proofs for block entries.

### Fields (owned)

None. Stateless — all data passed per call.

### Key Parameters (received per call)

| Parameter | Type | Used By |
|-----------|------|---------|
| `block` | `const Block&` | getInclusionProof, verifyInclusionProof |
| `entryIndex` | `size_t` | getInclusionProof |
| `leafHash` | `const std::string&` | verifyInclusionProof |
| `proofArray` | `const nlohmann::json&` | verifyInclusionProof |

### Relationships

- **Owned by**: Blockchain\<ChunkHandler\> (composition)
- **Depends on**: Block, StreamEntry, MerkleTree (namespace functions)
- **No dependency on**: ChainPersistence, DifficultyEngine, chunk types

### Validation Rules

- Entry index must be < block.entries.size()
- Proof verification recomputes root from leaf + proof path; compares against block.merkleRoot

---

## Entity: Blockchain\<ChunkHandler\> (Core — Modified)

**Purpose**: Orchestrates chain operations (publish, append, replace), stream operations, and delegates to owned modules.

### Fields (owned — shared state)

| Field | Type | Accessed By |
|-------|------|-------------|
| `chain` | `std::vector<ChunkHandler>` | Core, ChainPersistence |
| `keyIndexMap` | `std::map<std::string, std::vector<size_t>>` | Core, ChainPersistence |
| `streamRegistry` | `std::set<std::string>` | Core, ChainPersistence |
| `streamKeyIndex` | `std::map<...>` | Core, ChainPersistence |
| `blockchainPath` | `std::filesystem::path` | Core, ChainPersistence |
| `config` | `ConsensusConfig` | Core, DifficultyEngine (by ref) |
| `currentDifficulty` | `uint32_t` | Core, DifficultyEngine (by ref) |
| `dirty_` | `bool` | Core, ChainPersistence |
| `totalBlockCount_` | `size_t` | Core, ChainPersistence, DifficultyEngine |
| `chunkCount_` | `size_t` | Core, ChainPersistence |
| `difficultyCache_` | `std::unordered_map<size_t, uint32_t>` | Core, DifficultyEngine, ChainPersistence (recovery clears) |
| `retainedChunks_` | `std::set<size_t>` | Core, ChainPersistence (freeChunk) |

### Fields (owned — module instances)

| Field | Type |
|-------|------|
| `persistence_` | `ChainPersistence<ChunkHandler>` |
| `difficultyEngine_` | `DifficultyEngine` |
| `proofService_` | `MerkleProofService` |

### Fields (owned — timer)

| Field | Type |
|-------|------|
| `io_context_` | `boost::asio::io_context*` |
| `save_timer_` | `std::shared_ptr<boost::asio::steady_timer>` |
| `save_interval_seconds_` | `uint32_t` |

### Relationships

- **Implements**: IBlockchain (unchanged interface)
- **Owns**: ChainPersistence, DifficultyEngine, MerkleProofService
- **Depends on**: Block, Chunk/MockChunk (template param), StreamEntry, ConsensusConfig

### Delegation Pattern

| IBlockchain Method | Delegates To |
|-------------------|-------------|
| `saveChunk()`, `loadChunk()`, `freeChunk()` | `persistence_` |
| `saveKeys()`, `loadKeys()` | `persistence_` |
| `saveAllChunks()`, `recoverChain()`, `archiveChainFiles()` | `persistence_` |
| `discoverChunks()`, `validateChunk()` | `persistence_` |
| `calculateNewDifficulty()` | `difficultyEngine_` |
| `getDifficultyForHeight()` | `difficultyEngine_` |
| `getInclusionProof()` | `proofService_` |
| `verifyInclusionProof()` | `proofService_` |
| All other methods | Handled directly in core |
