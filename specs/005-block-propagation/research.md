# Research: Block Propagation & Validation on Receipt

**Feature**: 005-block-propagation | **Date**: 2026-04-10

## R1: Block Relay Deduplication Strategy

**Decision**: Use an in-memory `std::unordered_set<std::string>` of block hashes with a bounded capacity (512 entries) and FIFO eviction via a companion `std::deque`.

**Rationale**: The block hash is already computed during validation (via `Block::calculateHash()` / `IBlockchain::isValidNewBlock()`), so using it as the dedup key has zero additional computational cost. An unordered set gives O(1) lookup. A deque of insertion order enables O(1) eviction of the oldest entry when the set reaches capacity.

**Alternatives considered**:
- **LRU cache (std::list + map)**: More complex, unnecessary since access pattern is insert-and-check, not access-frequency-based.
- **Bloom filter**: Probabilistic — false positives would suppress legitimate blocks. Unacceptable for a blockchain.
- **Block index as key**: Does not distinguish blocks in fork scenarios (same index, different hash). Rejected per clarification.

**Size justification**: At 10 blocks/sec max throughput, 512 entries covers ~50 seconds of history. Blocks older than that would have already propagated through the entire network. SHA-256 hash strings are 64 bytes each; 512 entries ≈ 33 KB memory overhead — negligible.

## R2: Bounded Pending Pool for Gap Blocks

**Decision**: Use a `std::unordered_map<std::string, PendingBlock>` keyed by `prevHash` (the hash the block is waiting for), with a max capacity of 64 entries and a per-entry TTL of 60 seconds.

**Rationale**: When a block arrives whose `prevHash` doesn't match the current chain tip, it's placed in the pending pool. When a new block is successfully appended to the chain, the pool is checked for entries whose `prevHash` matches the newly appended block's `hash`. This creates a chain-resolution mechanism without requiring explicit sync requests.

**Alternatives considered**:
- **Keyed by block.index**: Doesn't help resolve the chain — we need to know *which predecessor* the block depends on, not its position.
- **Unbounded queue**: Memory exhaustion risk from malicious peers flooding gap blocks. Rejected.
- **No pending pool (always discard)**: Simpler but leads to more sync round-trips. User chose deferral during clarification.

**Eviction policy**: When capacity is full, evict the oldest entry (by insertion time). Entries exceeding TTL are lazily evicted during insertion or chain-append checks. No background timer needed — avoids complexity.

**PendingBlock struct**:
- `Block block` — the deferred block
- `std::string sender_key` — "host:port" of the peer that sent it (for relay exclusion if resolved)
- `std::chrono::steady_clock::time_point inserted_at` — for TTL expiry

## R3: Per-Peer Rate Limiting Pattern

**Decision**: Token-bucket rate limiter per peer, implemented as a simple counter with a sliding window. Each peer gets a `BlockRateState` tracking the count of BLOCK packets received in the current 1-second window.

**Rationale**: Boost.Asio's single-threaded `io_context` model means rate tracking doesn't need atomics or locks — all packet handling runs on the same strand. A 1-second sliding window with a limit of 10 blocks/second (matching SC-007) is simple to implement and reason about.

**Implementation sketch**:
- On each inbound BLOCK packet, check `rate_state[peer_key].count` for the current window.
- If `count >= limit`, drop the block, increment error count, log warning.
- If the window has expired (current time - window_start > 1s), reset count to 0.
- No background timer; window is checked lazily on each packet arrival.

**Alternatives considered**:
- **Global rate limit**: Doesn't prevent a single peer from consuming the entire budget. Rejected per clarification.
- **Leaky bucket**: More complex state tracking with no benefit at this throughput level.
- **Asio timer per peer**: Overhead of managing timers for each peer; lazy check is sufficient.

## R4: Integration with Existing Architecture

**Decision**: The `BlockPropagation` class is a standalone component owned by `main()` and passed as a dependency to `PeerManager`, `PeerServer`, and `RpcServer`. All propagation logic runs on the existing single-threaded `io_context`.

**Rationale**: The existing codebase uses a single `io_context` with async Boost.Asio handlers. Block propagation can be integrated without threads or locks by running all propagation logic within the same event loop. This matches the existing patterns in `PeerServer::do_read_body()` and `PeerClient::do_read_body()`.

**Integration points**:
1. **Outbound broadcast (FR-001)**: After `Blockchain::addBlock()` returns in `RpcServer`, call `PeerManager::broadcast_block(block)` which iterates `outbound_connections_` and sends via `PeerClient::send(block, BLOCK)`.
2. **Inbound validation (FR-002, FR-003)**: In `PeerServer::do_read_body()` BLOCK case and `PeerClient::do_read_body()` BLOCK case, call `BlockPropagation::on_block_received(block, sender_key)` which validates, appends, deduplicates, and triggers relay.
3. **Relay (FR-004)**: After successful append, `BlockPropagation` calls `PeerManager::relay_block(block, exclude_peer_key)` which sends to all connections except the sender.
4. **Sync queue (FR-012)**: `BlockPropagation::on_block_received()` checks `SyncStatus::isSyncing` — if true, enqueues; when sync completes, a callback processes the queue.

**Callback for addBlock**: Rather than modifying the `Blockchain` template directly, the `RpcServer` calls propagation after `addBlock()` returns the new block. This avoids coupling the blockchain data structure to network concerns.

**Alternatives considered**:
- **Observer pattern on Blockchain**: Would require modifying the `Blockchain` template class and `IBlockchain` interface. Too invasive for this feature.
- **Separate thread for propagation**: Adds synchronization complexity; unnecessary given existing single-threaded model.
- **Embedding propagation logic directly in PeerServer/PeerClient**: Violates separation of concerns. The same validation + relay logic is needed in both server and client handlers, so a shared component is cleaner.
