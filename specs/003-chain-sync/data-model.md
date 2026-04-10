# Data Model: Chain Synchronization

**Feature**: 003-chain-sync
**Date**: 2026-04-10

## New Entities

### SyncQuery

Represents a chain sync request sent from client to peer.

| Field | Type | Description |
|-------|------|-------------|
| local_chain_height | `uint64_t` | Number of blocks the requesting node currently has |

**Serialization**: Boost.Serialization binary archive (matches existing P2P wire format).

**Wire identification**: `PacketType::BLOCKCHAIN_QUERY` (already defined in `PacketHeader.hpp`).

### SyncResponse

Represents a chunk of chain data sent from peer to client in response to a sync query.

| Field | Type | Description |
|-------|------|-------------|
| total_chain_height | `uint64_t` | Total number of blocks the responding peer has |
| chunk_index | `uint64_t` | Index of the chunk being sent (0-based) |
| blocks | `vector<Block>` | Blocks in this chunk (up to 100, may be partial for the last chunk) |

**Serialization**: Boost.Serialization binary archive.

**Wire identification**: `PacketType::BLOCKCHAIN_RESPONSE` (already defined in `PacketHeader.hpp`).

**Relationship**: Each `SyncResponse` contains `Block` entities (existing). Multiple `SyncResponse` messages may be sent for a single `SyncQuery`.

### SyncState (enum)

Represents the synchronization state of a node.

| Value | Description |
|-------|-------------|
| IDLE | Not syncing. Normal operation. addBlock RPC permitted. |
| SYNCING | Actively downloading chain from a peer. addBlock RPC blocked. |

**Visibility**: Shared between `PeerClient` (sets state) and `RpcServer` (reads state for addBlock gating).

## Modified Entities

### IBlockchain (interface)

New members added to the interface:

| Member | Type | Description |
|--------|------|-------------|
| getChainBlockCount() | `size_t` | Returns total number of blocks across all chunks. Already exists in `Blockchain` but not in interface. Promote to interface. |

### Blockchain

Existing methods used by sync:

| Method | Role in Sync |
|--------|-------------|
| `replaceChain(vector<Block>)` | Atomically replaces the local chain with validated candidate blocks from peer |
| `isValidNewBlock(Block, Block, ConsensusConfig)` | Validates each incoming block during sync |
| `isValidChain(vector<Block>)` | Validates an entire candidate chain before replacement |
| `saveChunk(size_t)` | Persists a validated chunk to disk during incremental sync |
| `loadChunk(size_t)` | Loads chunk from disk (used during partial-chain reconstruction) |

### Block (unchanged)

Existing entity, no modifications. Serialization already includes all fields (`index`, `timestamp`, `data`, `prevHash`, `hash`, `nonce`, `difficulty`).

### Chunk (unchanged)

Existing entity, no modifications. Chunk boundaries (100 blocks) define the sync transfer unit.

## State Transitions

```
Node startup
  │
  ▼
IDLE ──(peer connect or requestSync RPC)──► SYNCING
  ▲                                            │
  │                                            │
  └──(sync complete / timeout / error)─────────┘
```

During SYNCING:
1. Send `SyncQuery` with local chain height
2. Receive `SyncResponse` (one chunk)
3. Validate all blocks in the chunk
4. If valid: persist chunk, request next chunk (or finish)
5. If invalid: discard chunk, abort sync, return to IDLE
6. If timeout (60s): abort sync, return to IDLE

## Validation Rules (applied during sync)

All validation uses existing logic — no new validation rules introduced:

1. **Per-block**: `IBlockchain::isValidNewBlock()` checks index continuity, hash linkage, hash correctness, difficulty minimum, proof-of-work, and timestamp bounds.
2. **Per-chain**: `Blockchain::isValidChain()` verifies genesis block structure and validates every block against its predecessor.
3. **Chain replacement**: `Blockchain::replaceChain()` enforces longest-chain rule (rejects equal-length or shorter candidates) and `maxReorgDepth` limit.
