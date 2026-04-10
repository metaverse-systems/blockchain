# P2P Binary Protocol Contract: Chain Synchronization

**Feature**: 003-chain-sync
**Date**: 2026-04-10
**Base contract**: [002 P2P binary contract](../../002-consensus-mechanism/contracts/p2p-binary.md)

This document describes **only the changes** to the P2P binary protocol introduced by chain synchronization. All existing packet types and behaviors are unchanged.

## New Packet Implementations

The `BLOCKCHAIN_QUERY` and `BLOCKCHAIN_RESPONSE` packet types were reserved in `PacketHeader.hpp` but not implemented. This feature implements both.

### BLOCKCHAIN_QUERY

**Direction**: Client → Server (requester → responder)

**Packet header**:
| Field | Value |
|-------|-------|
| `type` | `PacketType::BLOCKCHAIN_QUERY` (1) |
| `length` | Size of serialized payload |

**Payload** (Boost.Serialization binary archive):
| Field | Type | Description |
|-------|------|-------------|
| `local_chain_height` | `uint64_t` | Number of blocks the requester currently has |

**Trigger**: Sent automatically after TLS handshake completes on a new peer connection, or on demand via `requestSync` RPC.

### BLOCKCHAIN_RESPONSE

**Direction**: Server → Client (responder → requester)

**Packet header**:
| Field | Value |
|-------|-------|
| `type` | `PacketType::BLOCKCHAIN_RESPONSE` (2) |
| `length` | Size of serialized payload |

**Payload** (Boost.Serialization binary archive):
| Field | Type | Description |
|-------|------|-------------|
| `total_chain_height` | `uint64_t` | Total number of blocks the responder has |
| `chunk_index` | `uint64_t` | 0-based index of the chunk being sent |
| `blocks` | `vector<Block>` | Blocks in this chunk (up to 100; may be fewer for the last chunk) |

**Behavior**: The server sends one `BLOCKCHAIN_RESPONSE` per chunk that the client is missing. Multiple responses are sent sequentially for a multi-chunk gap.

**Empty response**: If the requester's chain height is equal to or greater than the responder's, no `BLOCKCHAIN_RESPONSE` is sent (the server sends a response with `total_chain_height` equal to or less than `local_chain_height` and empty `blocks` to signal "nothing to sync").

## Protocol Flow

```
Client                                    Server
  │                                          │
  │──── BLOCKCHAIN_QUERY ───────────────────►│
  │     { local_chain_height: 150 }          │
  │                                          │
  │◄─── BLOCKCHAIN_RESPONSE ────────────────│
  │     { total: 350, chunk: 1,              │
  │       blocks: [100..199] }               │
  │                                          │
  │     (client validates & persists chunk)   │
  │                                          │
  │◄─── BLOCKCHAIN_RESPONSE ────────────────│
  │     { total: 350, chunk: 2,              │
  │       blocks: [200..299] }               │
  │                                          │
  │     (client validates & persists chunk)   │
  │                                          │
  │◄─── BLOCKCHAIN_RESPONSE ────────────────│
  │     { total: 350, chunk: 3,              │
  │       blocks: [300..349] }               │
  │                                          │
  │     (client validates & persists chunk)   │
  │     (sync complete → IDLE)               │
```

## Validation on Receipt (Client Side)

For each received `BLOCKCHAIN_RESPONSE`:

1. Verify `total_chain_height > local_chain_height` (peer has a longer chain).
2. For each block in `blocks`, validate using `IBlockchain::isValidNewBlock()`.
3. If all blocks valid: persist chunk to disk, update local chain height, continue.
4. If any block invalid: discard the chunk, abort sync, log error.
5. If response not received within 60 seconds: abort sync, log timeout warning.

## Timeout

A 60-second deadline timer applies per `BLOCKCHAIN_RESPONSE`. If no response arrives within the window, the client aborts sync and returns to IDLE state.

## Changes in This Feature

- **FR-001**: `BLOCKCHAIN_QUERY` packet type implemented with chain height payload.
- **FR-002**: `BLOCKCHAIN_RESPONSE` packet type implemented with chunk-aligned block batches.
- **FR-003**: Query includes `local_chain_height` for incremental sync determination.
- **FR-004**: Responses are chunk-aligned (up to 100 blocks per response).
- **FR-005, FR-006**: Client validates each block per chunk; invalid chunks are rejected entirely.
- **FR-015**: 60-second per-chunk timeout enforced.
