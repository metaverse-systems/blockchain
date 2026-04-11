# Data Model: RPC API Expansion

**Feature**: 010-rpc-api-expansion  
**Date**: 2026-04-11

## Entities

### NodeStatus (read-only composite — not persisted)

A transient view assembled from multiple subsystem queries at request time.

| Field | Type | Source |
|-------|------|--------|
| chainLength | size_t | `IBlockchain::getChainLength()` |
| chunkCount | size_t | `IBlockchain::getChunkCount()` |
| syncState | string ("idle" \| "syncing") | `SyncStatus::isSyncing.load()` |
| currentDifficulty | uint32_t | `IBlockchain::getCurrentDifficulty()` (new on interface) |
| inboundPeers | size_t | `PeerManager::inbound_count()` |
| outboundPeers | size_t | `PeerManager::outbound_count()` |
| nodeUuid | string | `PeerManager::get_node_uuid()` |

**Relationships**: None. This is a snapshot assembled from existing subsystem state.
**Validation**: None — all fields are read from validated subsystem state.
**State transitions**: None — stateless read-only query.

### BlockRange (request parameters — not persisted)

| Field | Type | Constraints |
|-------|------|-------------|
| startIndex | size_t | Must be < chainLength |
| endIndex | size_t | Must be >= startIndex; clamped to chainLength - 1 if exceeds |
| headersOnly | bool (optional, default false) | When true, return header-only objects |

**Validation rules**:
1. `startIndex` must be a non-negative integer
2. `endIndex` must be a non-negative integer
3. `startIndex` must be <= `endIndex` (error -32602 otherwise)
4. `startIndex` must be < `chainLength` (error -32001 otherwise)  
5. `endIndex - startIndex + 1` must be <= 1000 (error -32602 otherwise)
6. `endIndex` is clamped to `chainLength - 1` when it exceeds chain length (silent, no error)

**Output**: Array of `Block::toJson()` or `Block::toHeaderJson()` objects depending on `headersOnly`.

## Interface Changes

### IBlockchain (src/IBlockchain.hpp)

**New virtual method**:
```
virtual uint32_t getCurrentDifficulty() const = 0
```

**Impact**: All classes implementing `IBlockchain` must provide this method.
- `Blockchain<ChunkHandler>` — already has the method, just needs `override`
- `MockBlockchain` (tests) — needs a stub returning a default value

### RpcServer (src/network/RpcServer.cpp)

**New method handlers** (4 additions to if-else dispatch chain):
- `getNodeStatus` — no parameters; returns JSON object
- `getBlockRange` — params: `{startIndex, endIndex, headersOnly?}`; returns JSON array
- `getChainLength` — no parameters; returns integer
- `getChunkCount` — no parameters; returns integer

No new response helper functions needed — existing `resultMessage()`, `resultJsonMessage()`, `invalidParamsMessage()`, and `errorMessage()` cover all cases.
