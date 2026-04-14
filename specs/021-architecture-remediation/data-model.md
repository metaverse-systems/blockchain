# Data Model: Architecture Remediation

**Feature**: 021-architecture-remediation  
**Date**: 2026-04-13

## Entities

### IChainReader (Interface — Widened)

Read-only query interface. All query methods consolidated here.

| Method | Return Type | Description |
|--------|-------------|-------------|
| `isShuttingDown()` | `bool` | Check shutdown state |
| `listStreams()` | `std::set<std::string>` | List all stream names |
| `getStreamEntries()` | `std::vector<std::pair<size_t, StreamEntry>>` | Get entries for a stream/key |
| `getStreamEntry()` | `std::pair<size_t, StreamEntry>` | Get single entry by stream+key |
| `getChainBlockCount()` | `size_t` | Total blocks across all chunks |
| `getChainLength()` | `size_t` | Loaded chain length |
| `getChunkCount()` | `size_t` | Number of chunks |
| `getCurrentDifficulty()` | `uint32_t` | Current mining difficulty |
| `getConfig()` | `const ConsensusConfig&` | Consensus configuration |
| `getBlockByIndex()` | `Block` | **MOVED FROM IBlockchain** |
| `getBlocksByKeys()` | `std::vector<Block>` | **MOVED FROM IBlockchain** |
| `getInclusionProof()` | `nlohmann::json` | **MOVED FROM IBlockchain** |
| `verifyInclusionProof()` | `nlohmann::json` | **MOVED FROM IBlockchain** |

**Relationship**: Implemented by `Blockchain<ChunkHandler>`. Consumed by `RpcServer` (read path), `PeerServer` (sync read path).

### IChainWriter (Interface — Updated Signature + Exception Contract)

Mutation interface for chain operations.

| Method | Return Type | Description | Exception on Failure |
|--------|-------------|-------------|---------------------|
| `generateGenesisBlock()` | `void` | Create genesis block | `PersistenceError` |
| `publish()` | `Block` | Publish data to stream | `ValidationError`, `PersistenceError` |
| `createStream()` | `void` | Create named stream | `ValidationError` |
| `appendBlock()` | `void` | Append validated block | `ValidationError` |
| `replaceChainStreaming(size_t candidateLength, std::function<std::vector<Block>(size_t, size_t)> fetcher)` | `void` | Streaming replacement — fetches blocks in batches via callback | `ValidationError`, `PersistenceError` |
| `setShuttingDown()` | `void` | Signal shutdown (never fails) | — |
| `saveKeys()` | `void` | Persist key indexes | `PersistenceError` |

**Relationship**: Implemented by `Blockchain<ChunkHandler>`. Consumed by `RpcServer` (publish/createStream only), `ChainService`.

### ChainService (New Entity)

Thin mediator between network and domain layers.

| Method | Parameters | Return Type | Description |
|--------|-----------|-------------|-------------|
| `submitBlock()` | `const Block&` | `void` | Validate, append, and persist a single block |
| `submitSyncBatch()` | `const std::vector<Block>&, size_t local_height` | `void` | Process a batch of sync'd blocks (overlap check, append new, persist) |
| `getChainHeight()` | — | `size_t` | Delegates to reader |
| `getBlockAtTip()` | — | `Block` | Returns last block for validation |
| `getConsensusConfig()` | — | `const ConsensusConfig&` | Delegates to reader |

**State**: Holds `IBlockchain&` reference. Stateless beyond that — no caching.

**Relationship**: Created in `main.cpp`. Injected into `PeerClient`, `PeerServer` (via `PeerManager`), `BlockPropagation`.

### ChainError Hierarchy (New Entities)

```
ChainError : std::runtime_error
├── ValidationError     — invalid blocks, streams, inputs
├── PersistenceError    — chunk/key save/load failures
└── PeerError           — peer add/remove/ban failures
```

| Exception | Base | Typical Throwers |
|-----------|------|-----------------|
| `ChainError` | `std::runtime_error` | Base class, not thrown directly |
| `ValidationError` | `ChainError` | `appendBlock`, `publish`, `createStream`, `replaceChain` |
| `PersistenceError` | `ChainError` | `saveChunk`, `saveKeys`, `loadChunk`, `saveAllChunks` |
| `PeerError` | `ChainError` | `add_peer`, `remove_peer` (when operation is invalid) |

### SyncResponse (Modified Entity)

| Field | Type | Change |
|-------|------|--------|
| `total_chain_height` | `uint64_t` | Unchanged |
| `chunk_index` | `uint64_t` | **REMOVED** |
| `start_index` | `uint64_t` | **NEW** — first block index in this batch |
| `blocks` | `std::vector<Block>` | Unchanged |

### SyncQuery (Unchanged)

| Field | Type |
|-------|------|
| `local_chain_height` | `uint64_t` |

## State Transitions

### Chain Replacement (Streaming)

```
IDLE → VALIDATING → COMMITTING → IDLE
                  ↘ ROLLING_BACK → IDLE
```

1. **IDLE → VALIDATING**: `replaceChain()` called. Difficulty cache cleared. Candidate blocks validated in chunks of 100, written to temp files.
2. **VALIDATING → COMMITTING**: All batches valid. Archive old chain files, rename temp files to permanent.
3. **VALIDATING → ROLLING_BACK**: Validation failure detected. Delete temp files, original chain untouched.
4. **COMMITTING → IDLE**: Rebuild in-memory indexes from new chain. Save keys/streams.

## Validation Rules

- `ChainService::submitBlock()`: Calls `IBlockchain::isValidNewBlock()` before `appendBlock()`. Throws `ValidationError` on failure.
- `ChainService::submitSyncBatch()`: Verifies overlap hashes match local chain. Throws `ValidationError` on fork detection.
- `replaceChain()` streaming: Candidate must be strictly longer than current chain. Reorg depth must not exceed `config.maxReorgDepth`. Each block validated individually.
