# P2P Binary Protocol Contract: Block Propagation & Validation on Receipt

**Feature**: 005-block-propagation
**Date**: 2026-04-10
**Base contract**: [004 P2P binary contract](../../004-peer-discovery/contracts/p2p-binary.md)

This document describes **only the changes** to the P2P binary protocol introduced by block propagation. All existing packet types and behaviors are unchanged.

## Modified Packet Types

### BLOCK (PacketType 0) — Updated Behavior

The `BLOCK` packet type (value `0`) already exists in `PacketHeader.hpp`. This feature defines its complete send and receive semantics for real-time block propagation.

**Direction**: Bidirectional (any connected peer may send or receive)

**Packet header**:
| Field | Value |
|-------|-------|
| `type` | `PacketType::BLOCK` (0) |
| `length` | Size of serialized payload |

**Payload** (Boost.Serialization binary archive):
| Field | Type | Description |
|-------|------|-------------|
| `index` | `size_t` | Block index |
| `timestamp` | `uint64_t` | Unix timestamp |
| `data` | `string` | Block data payload |
| `prevHash` | `string` | Hash of the previous block |
| `hash` | `string` | SHA-256 hash of this block |
| `nonce` | `uint64_t` | Proof-of-work nonce |
| `difficulty` | `uint32_t` | PoW difficulty |

This is the existing `Block` struct serialized via Boost.Serialization. No new fields.

## Protocol Flows

### Outbound Block Broadcast (local block creation)

```
Node A (creates block)                   Node B (connected peer)
  │                                          │
  │ addBlock() via RPC                       │
  │   → block mined & appended locally       │
  │                                          │
  │──── BLOCK ──────────────────────────────►│
  │     { index, timestamp, data,            │
  │       prevHash, hash, nonce, difficulty } │
  │                                          │
  │                        validate block    │
  │                        append if valid   │
  │                        relay to peers    │
  │                        (excluding A)     │
```

Broadcast is sent to **all** connected peers simultaneously (both outbound `PeerClient` connections and inbound `PeerServer` sessions).

### Inbound Block Reception & Relay

```
Node A ────BLOCK────► Node B ────BLOCK────► Node C
                        │
                        ├─ dedup check (block hash in cache?)
                        │   └─ if duplicate → discard, done
                        │
                        ├─ rate limit check (per-peer)
                        │   └─ if exceeded → drop, increment error
                        │
                        ├─ sync check
                        │   └─ if syncing → enqueue, done
                        │
                        ├─ validate (IBlockchain::isValidNewBlock)
                        │   ├─ valid → append, add to dedup cache
                        │   │          relay to peers except A
                        │   │          check pending pool
                        │   └─ invalid → reject, increment A's error
                        │
                        └─ if prevHash doesn't match tip
                            └─ insert into pending pool
```

### Block Received During Sync

```
Node A ─── BLOCKCHAIN_RESPONSE (sync) ──► Node B
                                            │ (sync in progress)
Node C ─── BLOCK ──────────────────────► Node B
                                            │
                                            ├─ dedup check
                                            ├─ rate limit check
                                            └─ enqueue in SyncBlockQueue
                                               (bounded, max 128 entries)

    ... sync completes ...

Node B: process SyncBlockQueue
    │
    ├─ for each queued block:
    │   ├─ re-check dedup (may now be in chain from sync)
    │   ├─ validate
    │   ├─ append if valid
    │   └─ relay if valid
```

## Behavioral Changes

### PeerServer (inbound handler)

**Before (004)**: `BLOCK` case in `do_read_body()` deserializes and logs the block but takes no further action.

**After (005)**: `BLOCK` case calls `BlockPropagation::on_block_received(block, sender_key)` which handles dedup, rate limiting, sync queueing, validation, appending, and relay.

### PeerClient (outbound handler)

**Before (004)**: `BLOCK` case in `do_read_body()` deserializes and logs the block but takes no further action.

**After (005)**: `BLOCK` case calls `BlockPropagation::on_block_received(block, sender_key)` — same processing path as PeerServer.

### PeerManager

**New method**: `broadcast_block(const Block &block)` — sends the block to all connected peers (outbound and inbound).

**New method**: `relay_block(const Block &block, const std::string &exclude_key)` — sends the block to all connected peers except the one identified by `exclude_key`.

## Error Handling

| Scenario | Action |
|----------|--------|
| Deserialization failure on BLOCK payload | Log error, increment peer error count |
| Block fails validation | Reject (do not append), increment peer error count, do not relay |
| Rate limit exceeded | Drop block, increment peer error count |
| Pending pool at capacity | Evict oldest entry, insert new entry |
| SyncBlockQueue at capacity | Evict oldest entry, insert new entry |
