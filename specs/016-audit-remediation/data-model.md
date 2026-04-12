# Data Model: Code Audit Remediation

**Feature**: 016-audit-remediation  
**Date**: 2026-04-12

---

## New Entities

### DifficultyCache (in-memory, not persisted)

Added to `Blockchain` class as a member.

| Field | Type | Description |
|-------|------|-------------|
| `difficultyCache_` | `std::unordered_map<size_t, uint32_t>` | Maps adjustment boundary height → difficulty at that boundary |

**Lifecycle**:
- Populated lazily in `getDifficultyForHeight()` as boundaries are computed
- Cleared entirely on `replaceChain()` (chain history changed)
- Cleared on `recoverChain()` (chain reloaded from disk)
- Not invalidated by `publish()`/`appendBlock()` (new boundaries are additive)

**Validation**: None — values are computed from validated chain data.

---

### ChunkRetainSet (in-memory, transient)

Added to `Blockchain` class as a member.

| Field | Type | Description |
|-------|------|-------------|
| `retainedChunks_` | `std::set<size_t>` | Set of chunk indices that must not be freed |

**Lifecycle**:
- Populated by RAII guard (`ChunkRetainGuard`) at the start of multi-access operations
- Cleared by guard destructor, which also frees all retained chunks that are no longer needed
- Never persisted

**Validation**: None — indices are always valid chunk positions.

---

### PendingPool (modified structure in BlockPropagation)

Replaces current `std::unordered_map<std::string, PendingEntry> pending_pool_`.

| Field | Type | Description |
|-------|------|-------------|
| `pending_map_` | `std::unordered_map<std::string, PendingEntry>` | Block hash → pending entry for O(1) lookup |
| `pending_order_` | `std::deque<std::string>` | Block hashes in insertion order for O(1) eviction |

**Lifecycle**:
- Insert: push hash to `pending_order_` back, insert into `pending_map_`
- Evict oldest: pop from `pending_order_` front, erase from `pending_map_`
- Expire: walk `pending_order_` from front while expired, erase both
- Lookup: `pending_map_.contains(hash)`

**Validation**: Both structures must stay in sync — every hash in the deque must exist in the map.

---

## Modified Entities

### Block (modified constructor)

New constructor overload added for received/synced blocks:

```
Block(index, time, prev_hash, entries, nonce, difficulty, merkleRoot, hash)
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `merkleRoot` | `std::string` | Pre-computed merkle root from sender |
| `hash` | `std::string` | Pre-computed block hash from sender |

**Validation**: Constructor verifies merkle root against entries. Throws `std::invalid_argument` if verification fails. Hash is verified against block contents including the verified merkle root.

**State transition**: None — blocks are immutable after construction.

---

### Blockchain (modified members)

| Member | Change | Description |
|--------|--------|-------------|
| `difficultyCache_` | Added | See DifficultyCache entity above |
| `retainedChunks_` | Added | See ChunkRetainSet entity above |

---

### BlockPropagation (modified members)

| Member | Change | Description |
|--------|--------|-------------|
| `pending_pool_` | Replaced by `pending_map_` + `pending_order_` | See PendingPool entity above |

---

## Utility Functions (new)

### utils.hpp / utils.cpp

| Function | Signature | Description |
|----------|-----------|-------------|
| `chunkFilename` | `std::string chunkFilename(size_t index)` | Returns `"chunk_NNNNNN.dat"` with zero-padded 6-digit index |
| `parsePeerKey` | `std::pair<std::string, uint16_t> parsePeerKey(const std::string& key)` | Parses `host:port` or `[ipv6]:port` into host string and port number |

### RpcServer (new static methods)

| Function | Signature | Description |
|----------|-----------|-------------|
| `makeJsonRpcError` | `json makeJsonRpcError(json id, int code, const std::string& message, json data = nullptr)` | Constructs JSON-RPC error response |
| `makeJsonRpcResult` | `json makeJsonRpcResult(json id, json result)` | Constructs JSON-RPC success response |

### PeerManager (modified methods)

| Method | Change | Description |
|--------|--------|-------------|
| `broadcast_block` | Removed | Replaced by `send_to_peers` |
| `relay_block` | Removed | Replaced by `send_to_peers` |
| `send_to_peers` | Added | `void send_to_peers(const Block& block, const std::string& exclude_key = "")` |

### TestHelpers.hpp (new, test-only)

| Function | Signature | Description |
|----------|-----------|-------------|
| `createTestDir` | `std::filesystem::path createTestDir(const std::string& name)` | Creates temp directory for test isolation |
| `cleanupTestDir` | `void cleanupTestDir(const std::filesystem::path& dir)` | Removes temp directory |
| `defaultConsensusConfig` | `ConsensusConfig defaultConsensusConfig()` | Returns config with difficulty=0 |
| `mineTestBlock` | `Block mineTestBlock(...)` | Mines a block with given parameters |
| `buildValidChain` | `std::vector<Block> buildValidChain(size_t length, ...)` | Builds a valid chain of N blocks |

---

## Relationships

```
Blockchain
├── has-a DifficultyCache (1:1, in-memory)
├── has-a ChunkRetainSet (1:1, transient per operation)
├── has-many Chunk (1:N, persisted)
└── has-many Block (via Chunks)

BlockPropagation
├── has-a PendingPool (1:1, map + deque)
└── uses parsePeerKey() for sender key parsing

PeerManager
└── uses send_to_peers() (unified from broadcast/relay)

RpcServer
└── uses makeJsonRpcError/makeJsonRpcResult (shared helpers)
```
