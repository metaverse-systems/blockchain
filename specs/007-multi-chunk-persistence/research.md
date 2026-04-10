# Research: Multi-Chunk Persistence & Recovery

**Feature**: 007-multi-chunk-persistence  
**Date**: 2026-04-10

## R1: Chunk File Discovery on Startup

**Decision**: Enumerate chunk files by iterating candidate names `chunk_000000.dat`, `chunk_000001.dat`, ... using `std::filesystem::exists()` until a gap is found.

**Rationale**: The codebase already uses explicit path construction (e.g., `Chunk.cpp` builds `chunk_NNNNNN.dat` from an index). Iterating by convention is simpler and more predictable than `directory_iterator` with filename parsing and sorting. It also naturally enforces contiguous-prefix loading — stop at the first missing file.

**Alternatives considered**:
- `std::filesystem::directory_iterator` + regex parse + sort: More flexible but adds complexity for no benefit since the naming convention is fixed.
- Store chunk count in a metadata file: Extra state to keep in sync; the file-presence approach is self-describing.

## R2: Periodic Timer Implementation

**Decision**: Use `boost::asio::steady_timer` with a recurring async wait, following the same pattern as `PeerManager::start_exchange_timer()`.

**Rationale**: The codebase already uses this pattern extensively for peer exchange (30s interval) and reconnection backoff. Boost.Asio timers integrate naturally with the existing `io_context::run()` event loop in `main.cpp` — no additional threads needed.

**Alternatives considered**:
- `std::thread` + `std::condition_variable` sleep loop: Introduces a second thread and requires explicit synchronization with the Asio event loop.
- `std::jthread` (C++20): Same threading concerns; doesn't integrate with the existing Asio reactor.

## R3: Dirty Tracking for Periodic Saves

**Decision**: Add a `bool dirty_` flag to the `Blockchain` class, set to `true` when a block is appended to the active chunk, and reset to `false` after a successful save. The periodic timer checks this flag before saving.

**Rationale**: Simple boolean is sufficient because only the active (latest) chunk is mutable. Filled chunks are immutable once saved. No per-chunk dirty tracking needed — only one chunk can be dirty at a time.

**Alternatives considered**:
- Per-chunk dirty flag on `IChunk`: Over-engineered since only the active chunk is ever modified in memory.
- Timestamp-based comparison (compare block timestamps to last save time): More complex and fragile.
- Hash-based comparison (hash chunk contents and compare to last saved hash): Expensive for no benefit.

## R4: Thread Safety — Periodic Save vs Block Append

**Decision**: The periodic save timer callback and block append operations all run within the Boost.Asio `io_context` event loop. Since the daemon uses a single `io_context::run()` call (single-threaded reactor pattern), all handlers are serialized by the event loop. No additional synchronization is needed.

**Rationale**: Examining `main.cpp`, there is a single `io_context.run()` call at the end. All async handlers (RPC, P2P, timers) are dispatched through this context. The periodic save timer callback will be dispatched the same way, ensuring mutual exclusion with block append handlers without explicit locks.

**Alternatives considered**:
- `boost::asio::strand`: Would be needed if `io_context::run()` were called from multiple threads; currently unnecessary but would be forward-compatible.
- `std::mutex` around save/append: Adds complexity and risk of deadlock in async code.

**Note**: If the daemon ever moves to multi-threaded `io_context::run()`, a strand will be required. This is documented as a known constraint.

## R5: Atomic File Saves (Write-to-Temp-Then-Rename)

**Decision**: Adopt the write-to-temp-then-rename pattern from `PeerManager::save_peers()` for chunk saves. Write to `chunk_NNNNNN.dat.tmp`, then `std::filesystem::rename()` to the final path.

**Rationale**: The current `Chunk::save()` writes directly to the final file. A crash mid-write would leave a truncated/corrupt file that would then fail to load on startup. The temp+rename pattern (already proven in `PeerManager`) ensures the final file is either the old complete version or the new complete version — never a partial write.

**Alternatives considered**:
- Write to final path with `fsync` then close: `fsync` guarantees the data hits disk but doesn't protect against partial writes if the process is killed mid-write.
- Journaling/WAL: Massively over-engineered for a chunk file that is written infrequently.

## R6: replaceChain Archive Strategy

**Decision**: On `replaceChain()`, create a `backups/<ISO-8601-UTC-timestamp>/` directory in the blockchain data directory, move all `chunk_*.dat`, `keys.dat`, `streams.dat`, and `stream_index.dat` files there using `std::filesystem::rename()`, then persist the new chain.

**Rationale**: Moving files is an O(1) operation on the same filesystem (rename only updates directory entries). No data is copied or deleted, minimizing I/O and preserving recoverability. ISO-8601 timestamps are lexicographically sortable, so operators can easily identify the most recent backup.

**Alternatives considered**:
- Delete old files outright: Irreversible; operator has no recovery path if the new chain turns out to be wrong.
- Copy instead of move: Doubles disk usage during the transition; move is cheaper and sufficient.
- Single `backup/` directory overwriting previous: Would lose the prior backup; timestamped dirs are cheap.

## R7: Index Rebuild on Missing Index Files

**Decision**: When any index file (`keys.dat`, `streams.dat`, `stream_index.dat`) is missing or fails to deserialize, rebuild that specific index by scanning all valid chunk files sequentially. This is a fallback path — normal startup trusts persisted indexes.

**Rationale**: Full index rebuild requires loading every chunk, which is expensive for large chains. Making it a fallback (not the default) keeps startup fast. Rebuilding per-index (not all-or-nothing) avoids unnecessary work when only one file is damaged.

**Alternatives considered**:
- Always rebuild all indexes: Safe but defeats the purpose of persisting them; startup for 10k+ blocks would be slow.
- Refuse to start if any index is missing: Too strict; the data is in the chunks and can be recovered.
- Store indexes inside chunk files: Would require a new serialization format and break backward compatibility with existing chunk files.
