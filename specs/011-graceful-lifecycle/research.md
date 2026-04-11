# Research: Graceful Multi-Chunk Shutdown & Startup

**Date**: 2026-04-11
**Feature**: 011-graceful-lifecycle

## Research Topic 1: Per-Chunk Dirty Tracking

### Decision
Add a `bool dirty_` flag to `IChunk` (the chunk base class) rather than tracking dirty state externally in `Blockchain`.

### Rationale
- `IChunk` is the natural location since dirty state is a property of individual chunks, not the chain.
- The flag is set whenever `push_back`, `emplace_back`, or `resize` modify the chunk's block vector.
- `Chunk::save()` clears the flag on successful write.
- `Chunk::load()` clears the flag since a freshly loaded chunk matches its on-disk state.
- `Blockchain::saveAllChunks()` iterates all chunks and saves only those with `dirty_ == true`.
- The existing `Blockchain::dirty_` flag (chain-level) is retained for the periodic save timer trigger but becomes a secondary signal; the per-chunk flag is authoritative for which chunks to persist.

### Alternatives Considered
1. **External dirty set in Blockchain** (e.g., `std::set<size_t> dirtyChunks_`): Rejected because it requires Blockchain to intercept every chunk mutation, creating coupling. The flag belongs on the chunk itself.
2. **Hash-based change detection** (compute hash of chunk contents and compare to last saved): Rejected as unnecessarily expensive — O(n) per chunk per save.

## Research Topic 2: Block Ingestion Freeze on Shutdown

### Decision
Add a `bool shutting_down_` flag to `IBlockchain`. On shutdown signal, set the flag before saving. `appendBlock()` and `publish()` check the flag and reject new blocks (throw or return early). `BlockPropagation::on_block_received()` checks the flag via the `IBlockchain` reference it already holds.

### Rationale
- The Boost.Asio single-threaded model means the signal handler callback runs as an event in the io_context — no other handlers run concurrently. However, the user explicitly requested an ingestion freeze for defense-in-depth.
- A simple boolean flag checked at entry to `appendBlock` / `publish` / `on_block_received` is sufficient without mutexes because the Asio loop is single-threaded.
- The flag is set in the signal handler lambda *before* `saveAllChunks()` is called.
- Rejected adding acceptor close or connection teardown — too disruptive and unnecessary for this feature's scope.

### Alternatives Considered
1. **Close P2P acceptor on shutdown**: Rejected — prevents further connections but doesn't stop in-flight message processing. Also adds complexity to restart paths.
2. **No freeze (rely on Asio single-thread guarantee)**: Rejected by user in clarification session. The flag is cheap and provides explicit safety.

## Research Topic 3: Fast Startup (Skip Validation)

### Decision
Add `bool fast_startup = false` to `PersistenceConfig` in `NodeConfig.hpp`. When enabled, `recoverChain()` skips the per-chunk `validateChunk()` call and cross-chunk linkage check, loading chunks by sequential file discovery only.

### Rationale
- The existing `discoverChunks()` already counts sequential chunk files without loading them. This is the "fast" path — it tells us how many chunks exist.
- The existing `recoverChain()` has a validation loop (`validateChunk(i)` + cross-chunk linkage). The fast path skips this loop entirely.
- Both paths converge on the same final state: placeholders for historical chunks, active (last) chunk loaded, indexes loaded/rebuilt.
- Config-driven (not CLI-only) so it persists across restarts.
- Default is `false` (safe) — operators must explicitly opt in.

### Alternatives Considered
1. **CLI flag only (not in config.json)**: Rejected because a persistent config option is more convenient for operators who always want fast startup.
2. **Partial validation (checksum-only without full deserialization)**: Rejected because the chunk format (Boost.Serialization binary archive) doesn't have a separate checksum — validation requires deserialization.

## Research Topic 4: Existing Code Patterns to Extend

### Current saveAllChunks() Behavior
```
saves only chain.back() (active chunk)
saves keys, streams, stream_index
sets dirty_ = false
```
Must be extended to iterate all chunks and save those with per-chunk dirty flag set.

### Current recoverChain() Behavior
```
discovers chunk files → validates each → checks cross-chunk linkage
loads only active chunk into memory → rebuilds indexes if needed
frees historical chunks
```
Already supports the placeholder + active-chunk model. Extension points:
- Add fast_startup bypass around the validation loop
- The final state (placeholders + active + indexes) is identical for both paths

### Current Signal Handler
```
stopPeriodicSave() → save_peers() → saveAllChunks() → io_context.stop()
```
Must be extended to:
```
set shutting_down_ = true → stopPeriodicSave() → save_peers() → saveAllChunks() → io_context.stop()
```

### replaceChain() and Dirty Chunks
`replaceChain()` already saves all chunks immediately after replacement and clears `dirty_`. Per-chunk dirty tracking is automatically consistent because `replaceChain` calls `save()` on every chunk.
