# Data Model: Block Propagation & Validation on Receipt

**Feature**: 005-block-propagation | **Date**: 2026-04-10

## Entities

### Block (existing — no changes)

| Field | Type | Description |
|-------|------|-------------|
| index | `size_t` | Sequential block number |
| timestamp | `uint64_t` | Unix timestamp of block creation |
| data | `std::string` | Opaque payload |
| prevHash | `std::string` | SHA-256 hash of the previous block |
| hash | `std::string` | SHA-256 hash of this block |
| nonce | `uint64_t` | Proof-of-work nonce |
| difficulty | `uint32_t` | PoW difficulty target |

Serialization: Boost.Serialization binary archive (existing). No changes required.

### PendingBlock (new)

Holds a block that arrived before its predecessor, pending resolution.

| Field | Type | Description |
|-------|------|-------------|
| block | `Block` | The deferred block |
| sender_key | `std::string` | `"host:port"` of the peer that sent it |
| inserted_at | `steady_clock::time_point` | Insertion time for TTL eviction |

**Lifecycle**: Created when a block's `prevHash` doesn't match the current chain tip. Resolved when the predecessor is appended. Evicted when TTL (60s) expires or pool capacity (64) is reached.

### BlockRateState (new)

Per-peer rate tracking for inbound BLOCK packets.

| Field | Type | Description |
|-------|------|-------------|
| count | `uint32_t` | Number of BLOCK packets in the current window |
| window_start | `steady_clock::time_point` | Start of the current 1-second window |

**Lifecycle**: Created on first BLOCK from a peer. Reset when the window expires. Removed when the peer disconnects.

### RecentBlockCache (new)

Bounded deduplication cache of recently seen block hashes.

| Field | Type | Description |
|-------|------|-------------|
| seen | `std::unordered_set<std::string>` | Set of block hashes |
| order | `std::deque<std::string>` | Insertion order for FIFO eviction |
| max_size | `size_t` | Capacity limit (default: 512) |

**Operations**:
- `contains(hash) → bool` — O(1) lookup
- `insert(hash)` — O(1) insert; if at capacity, evict oldest from deque and set
- No persistence — ephemeral across restarts

### SyncBlockQueue (new)

Bounded queue of blocks received during active chain sync.

| Field | Type | Description |
|-------|------|-------------|
| queue | `std::deque<std::pair<Block, std::string>>` | Queued (block, sender_key) pairs |
| max_size | `size_t` | Capacity limit (default: 128) |

**Lifecycle**: Blocks enqueued while `SyncStatus::isSyncing == true`. Queue processed after sync completes. If capacity reached, oldest entries are evicted.

## Relationships

```text
PeerServer / PeerClient
    │ receives BLOCK packet
    ▼
BlockPropagation
    ├── RecentBlockCache  ── dedup check
    ├── SyncBlockQueue    ── hold during sync
    ├── BlockRateState    ── per-peer rate check
    ├── PendingBlock pool ── defer gap blocks
    ├── IBlockchain       ── validate + append
    └── PeerManager       ── relay to other peers
```

## State Transitions

### Block Reception Flow

```text
BLOCK received
    │
    ├─ rate limit exceeded? → DROP, increment error
    │
    ├─ already in dedup cache? → DISCARD (silent)
    │
    ├─ sync in progress? → ENQUEUE in SyncBlockQueue
    │
    ├─ prevHash matches chain tip?
    │   ├─ YES → VALIDATE
    │   │         ├─ valid → APPEND to chain
    │   │         │          ├─ add hash to dedup cache
    │   │         │          ├─ RELAY to peers (exclude sender)
    │   │         │          └─ CHECK pending pool for children
    │   │         └─ invalid → REJECT, increment peer error
    │   │
    │   └─ NO → INSERT into pending pool (if capacity allows)
```

### Pending Pool Resolution

```text
Block appended to chain
    │
    └─ check pending pool: any entry with prevHash == new_block.hash?
        ├─ YES → remove from pool, run validation flow
        │        (may cascade: resolved block may unblock more)
        └─ NO  → done
```

## Validation Rules (existing — applied to received blocks)

All validation is performed by `IBlockchain::isValidNewBlock()`:

1. `newBlock.index == previousBlock.index + 1`
2. `newBlock.prevHash == previousBlock.hash`
3. `newBlock.calculateHash() == newBlock.hash`
4. `newBlock.difficulty >= config.minDifficulty` (non-genesis)
5. `checkLeadingZeroBits(newBlock.hash, newBlock.difficulty)` (non-genesis)
6. `newBlock.timestamp <= now + config.maxFutureTimestamp` (non-genesis)
