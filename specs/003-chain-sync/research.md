# Research: Chain Synchronization

**Feature**: 003-chain-sync
**Date**: 2026-04-10

## R1: Wire Format for BLOCKCHAIN_QUERY and BLOCKCHAIN_RESPONSE

**Decision**: Use Boost.Serialization binary archives for both query and response payloads, matching the existing `BLOCK` packet format. Query carries a `uint64_t` chain height. Response carries a serialized `vector<Block>` (one chunk's worth) plus `uint64_t` total chain height and `uint64_t` chunk index.

**Rationale**: The existing P2P protocol already uses `PacketHeader` (length + type) followed by a Boost.Serialization binary payload for `BLOCK` packets. Using the same serialization approach for sync messages avoids introducing a second wire format and keeps the codebase consistent. The response is chunk-aligned (up to 100 blocks) so that the receiver can persist each chunk directly to disk without reassembly.

**Alternatives considered**:
- JSON over TLS: Rejected — would create inconsistency with the existing binary protocol, and JSON serialization of block data is significantly larger than binary. Would also require adding JSON serialization for `Block` on the P2P channel (currently only used for RPC).
- Protobuf / FlatBuffers: Rejected — constitution principle V (minimal dependencies) prohibits adding new dependencies without explicit approval. Boost.Serialization is already approved.
- Single monolithic response: Rejected — sending the entire chain in one response would require the sender to load all chunks into memory simultaneously. Chunk-at-a-time transfer aligns with the existing storage model and supports incremental persistence (FR-009).

**Wire format**:
```
BLOCKCHAIN_QUERY payload:
  uint64_t  local_chain_height   // Number of blocks the requester has

BLOCKCHAIN_RESPONSE payload:
  uint64_t  total_chain_height   // Sender's total block count
  uint64_t  chunk_index          // Which chunk this response contains
  vector<Block> blocks           // The blocks in this chunk (Boost.Serialization)
```

## R2: Sync State Machine in PeerClient

**Decision**: Implement a simple linear state machine in `PeerClient`: IDLE → SYNCING → IDLE. On connect (or manual RPC trigger), transition to SYNCING, send `BLOCKCHAIN_QUERY`, receive chunked responses sequentially, validate and persist each chunk, and return to IDLE.

**Rationale**: The sync protocol is inherently request-response: the client asks "what am I missing?" and the server responds with chunk-by-chunk data. A linear state machine is the simplest correct model. There is no need for parallel chunk downloads since the P2P connection is a single TLS stream and the target network is 2–10 nodes with modest chain sizes.

**Alternatives considered**:
- No state machine (ad-hoc flags): Rejected — makes it hard to reason about when sync is active (needed for FR-016 addBlock blocking). An explicit state provides a single authoritative source.
- Parallel multi-peer sync: Rejected — the current codebase has a single `PeerClient` connecting to one peer. Multi-peer sync belongs to spec 004 (peer discovery). Over-engineering for current scope.
- Push-based sync (server sends unsolicited): Rejected — requires the server to track each client's chain state, adding complexity. Pull-based is simpler and gives the client control over pacing.

**States**:
| State   | Description | Transitions |
|---------|-------------|-------------|
| IDLE    | Not syncing. Normal operation. | → SYNCING (on connect or RPC trigger) |
| SYNCING | Actively downloading chunks from peer. `addBlock` RPC blocked. | → IDLE (sync complete, timeout, or error) |

## R3: Chunk-Aligned Transfer Strategy

**Decision**: Transfer whole chunks (up to 100 blocks each). The requester sends its chain height; the responder calculates which chunks are missing and sends them one at a time. The requester validates and persists each chunk before requesting the next.

**Rationale**: The existing storage model persists blocks in chunk files of 100 blocks each. Aligning sync transfer units to chunks means received data can be written directly to a chunk file without splitting or reassembly. This also provides natural batching — validating 100 blocks at a time is efficient, and rejecting per-chunk (not per-block or per-sync) keeps the granularity manageable per the clarified FR-006.

**Alternatives considered**:
- Block-at-a-time: Rejected — would require 1,000 round-trips for a 1,000-block chain. Even on local network, the round-trip overhead would likely exceed the 60-second SC-001 target.
- Full chain in one shot: Rejected — doesn't align with chunk persistence model and requires loading the entire chain into memory on the sender side.
- Variable-size batches: Rejected — adds complexity for tuning batch size. Chunk boundaries are a natural, fixed partition.

**Protocol flow**:
1. Client sends `BLOCKCHAIN_QUERY` with `local_chain_height = N`.
2. Server determines the first missing chunk: `start_chunk = N / 100`.
3. Server sends `BLOCKCHAIN_RESPONSE` for `start_chunk`, then for each subsequent chunk, until all chunks are sent.
4. Client validates each chunk's blocks sequentially against the previous block (using `isValidNewBlock`).
5. Client persists each validated chunk to disk immediately.
6. After the last chunk, client transitions to IDLE.

**Partial chunk handling**: If the client's chain height is not chunk-aligned (e.g., height=150 means 1.5 chunks), the server sends the partial chunk containing only blocks the client is missing. The client appends these blocks to its existing partial chunk.

## R4: Sync Trigger Integration Points

**Decision**: Two trigger points: (1) automatic on `PeerClient::connect()` handshake completion, and (2) on-demand via a new `requestSync` JSON-RPC method.

**Rationale**: Automatic sync on connect satisfies FR-007 (no manual intervention). The RPC trigger satisfies FR-017 (operator recovery tool). Both triggers invoke the same sync logic, just from different call sites.

**Alternatives considered**:
- Timer-based periodic sync: Rejected — unnecessary polling overhead when the node is already connected and in sync. The connect trigger handles the common case; the RPC trigger handles edge cases.
- Sync on every received block: Rejected — that's block propagation (spec 005), not chain sync. Over-architecting.

**RPC method**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "requestSync",
  "params": {}
}
```
Response: `{"jsonrpc": "2.0", "id": "1", "result": "sync_started"}` or an error if already syncing.

## R5: Sync-Aware RPC Behavior

**Decision**: Add an `isSyncing` flag (atomic boolean) to the blockchain or a shared sync state object, readable by `RpcServer`. When true, `addBlock` returns a JSON-RPC error; read-only methods proceed normally.

**Rationale**: The flag must be visible across the Asio event loop since both `PeerClient` (sync logic) and `RpcServer` (request handling) run on the same `io_context`. A `std::atomic<bool>` is the simplest thread-safe mechanism, though in a single-threaded Asio model, even a plain bool suffices since operations are serialized by the event loop.

**Alternatives considered**:
- Mutex-protected state: Rejected — heavier than needed for a single flag. Both PeerClient and RpcServer share the same io_context, so an atomic or plain bool suffices.
- RPC queue with sync priority: Rejected — over-engineering. A simple flag achieves FR-016 without altering the existing RPC dispatch model.

**Error response for addBlock during sync**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32001,
    "message": "Node is syncing",
    "data": "addBlock is unavailable while chain synchronization is in progress"
  }
}
```

## R6: Timeout and Error Handling During Sync

**Decision**: Apply a 60-second per-chunk Asio deadline timer. On timeout, abort sync from that peer and return to IDLE. On network error mid-sync, preserve all already-persisted chunks and return to IDLE.

**Rationale**: The 60-second timeout (per clarification Q3) is applied per chunk response, not for the entire sync. This means a 10-chunk sync gets up to 600 seconds total, but any single unresponsive chunk triggers an abort. Using Asio's `steady_timer` (already used for session timeouts in `SessionHandler`) provides a consistent approach.

**Alternatives considered**:
- Global sync timeout: Rejected — a fixed global timeout would need to scale with chain length, requiring the node to know how long sync "should" take. Per-chunk is self-adjusting.
- Retry with exponential backoff: Rejected for this spec — retry logic belongs with peer management (spec 004). For now, a simple abort-to-IDLE is sufficient. The operator can use `requestSync` RPC to retry manually.

**Error recovery**:
| Error | Behavior |
|-------|----------|
| Chunk timeout (60s) | Abort sync, return to IDLE, log warning |
| Connection drop | Abort sync, preserve persisted chunks, return to IDLE, log error |
| Invalid chunk | Discard invalid chunk, abort sync from this peer, return to IDLE, log error |
| Already syncing | `requestSync` RPC returns error; automatic sync on connect is skipped |
