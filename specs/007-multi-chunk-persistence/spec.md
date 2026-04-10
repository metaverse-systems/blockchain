# Feature Specification: Multi-Chunk Persistence & Recovery

**Feature Branch**: `007-multi-chunk-persistence`  
**Created**: 2026-04-10  
**Status**: Draft  
**Input**: User description: "Implement 007 — Multi-Chunk Persistence & Recovery"

## Clarifications

### Session 2026-04-10

- Q: Corruption recovery strategy — load contiguous prefix only or skip gaps and continue? → A: Contiguous prefix only — stop at first missing/corrupt chunk; discard everything after the gap.
- Q: Memory strategy for large chains — keep all chunks in memory or load on demand? → A: On-demand loading — only the active (current) chunk stays resident; filled chunks are loaded from disk when queried and freed after use.
- Q: Index rebuild strategy on startup — trust persisted index files or rebuild from chunks? → A: Trust persisted indexes — load existing index files; only rebuild by scanning chunks if index files are missing or corrupted.
- Q: What happens to existing chunk files when `replaceChain` is called? → A: Archive and rewrite — move old chunk files to a timestamped backup subdirectory within the blockchain data directory, then persist the new chain's chunks. No files are deleted.
- Q: Where should the periodic save interval be configured? → A: `config.json` — add a `persistence.save_interval_seconds` field (default 300, 0 to disable).

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Automatic Chunk Persistence on Fill (Priority: P1)

As a node operator running a blockchain that grows beyond a single chunk (100 blocks), I need filled chunks to be automatically saved to disk so that no data is lost on shutdown or crash.

**Why this priority**: Without this, any chain longer than 100 blocks silently loses all blocks beyond chunk 0 on restart, making the node functionally broken for production use.

**Independent Test**: Can be fully tested by adding 101+ blocks to a chain, shutting down the node, and verifying all blocks are present after restart.

**Acceptance Scenarios**:

1. **Given** a node with 100 blocks in chunk 0, **When** the 101st block is added, **Then** chunk 0 is automatically saved to disk before the new chunk 1 begins receiving blocks.
2. **Given** a node with multiple filled chunks on disk, **When** the node shuts down (SIGINT/SIGTERM), **Then** all in-memory chunks — including the current partially-filled chunk — are persisted to disk.
3. **Given** a node that was forcibly killed (SIGKILL / power loss) mid-operation, **When** the node restarts, **Then** all previously auto-saved filled chunks are intact and only the partial (unsaved) chunk may be lost.

---

### User Story 2 - Full Chain Recovery on Startup (Priority: P1)

As a node operator restarting a blockchain daemon, I need the startup sequence to automatically discover and load all persisted chunk files so the full chain state is restored without manual intervention.

**Why this priority**: Chunk persistence is useless without matching recovery logic. Both must ship together for the feature to deliver value.

**Independent Test**: Can be tested by creating a data directory with multiple pre-existing chunk files, starting the daemon, and verifying `getChainBlockCount` returns the correct total and that blocks from any chunk are queryable.

**Acceptance Scenarios**:

1. **Given** a blockchain data directory containing chunk files 0 through N, **When** the daemon starts, **Then** it discovers all N+1 chunk files, loads only the latest (active) chunk into memory, and reports the correct total block count.
2. **Given** a blockchain data directory with no existing chunk files, **When** the daemon starts, **Then** a fresh genesis block is created in chunk 0 as it does today.
3. **Given** a chain restored from disk, **When** a new block is published, **Then** it is appended to the correct chunk at the correct index, continuing seamlessly from the recovered state.
4. **Given** a restored chain with filled chunks on disk, **When** a query requests a block from a filled chunk, **Then** the system loads that chunk from disk, serves the block, and frees the chunk from memory after use.

---

### User Story 3 - Periodic Chunk Saving (Priority: P1)

As a node operator running a low-traffic chain where it may take hours or days to fill a chunk to capacity, I need the current partially-filled chunk to be periodically saved to disk so that an unexpected shutdown does not lose all blocks accumulated since the last full-chunk save.

**Why this priority**: On a low-throughput chain, a crash could lose nearly 100 blocks worth of work if the only save trigger is chunk-full. Periodic saving bounds the worst-case data loss to one save interval.

**Independent Test**: Can be tested by adding a handful of blocks, waiting for the periodic save interval to elapse, forcibly killing the daemon, and verifying those blocks survive on restart.

**Acceptance Scenarios**:

1. **Given** a node with a partially-filled current chunk, **When** the configured save interval elapses, **Then** the current chunk is saved to disk without interrupting normal operation.
2. **Given** a node that has had no new blocks since the last periodic save, **When** the save interval elapses again, **Then** no unnecessary disk write occurs (the system skips the save if the chunk has not changed).
3. **Given** a configurable save interval, **When** the operator sets it to a shorter or longer duration, **Then** the system respects the configured interval.
4. **Given** a periodic save in progress, **When** a new block arrives concurrently, **Then** the block is not lost or corrupted — either included in the current save or guaranteed to be in the next one.

---

### User Story 4 - Corrupted Chunk Detection and Reporting (Priority: P2)

As a node operator, I need the system to detect and clearly report corrupted chunk files on startup so I can take remedial action (re-sync from peers) rather than operating on silently corrupted data.

**Why this priority**: Data integrity validation is critical for a blockchain but can be delivered after the core save/load loop is working.

**Independent Test**: Can be tested by deliberately truncating or overwriting a chunk file with invalid bytes and verifying the daemon logs an error identifying the corrupted chunk.

**Acceptance Scenarios**:

1. **Given** a chunk file that has been truncated or contains invalid serialization data, **When** the daemon attempts to load it on startup, **Then** the system logs an error identifying the chunk number and file path, and skips the corrupted chunk.
2. **Given** a chunk file whose deserialized blocks fail hash validation, **When** the daemon loads it, **Then** the system logs a warning identifying which block indices are invalid.
3. **Given** one or more corrupted chunks in a sequence, **When** the daemon starts, **Then** only the contiguous prefix of valid chunks before the first corruption is loaded, and the operator is informed which chunks were discarded.

---

### User Story 5 - Chain Length and Chunk Count Introspection (Priority: P2)

As a developer or operator, I need `getChainLength` and `getChunkCount` methods on the blockchain interface so I can monitor chain growth and verify recovery completeness.

**Why this priority**: Essential for verifying the other stories work correctly and for future RPC/monitoring features, but lower priority than the core persistence mechanics.

**Independent Test**: Can be tested by adding a known number of blocks and verifying the returned counts match expectations.

**Acceptance Scenarios**:

1. **Given** a blockchain with 250 blocks across 3 chunks, **When** `getChainLength` is called, **Then** it returns 250.
2. **Given** a blockchain with 250 blocks across 3 chunks, **When** `getChunkCount` is called, **Then** it returns 3.
3. **Given** a freshly initialized blockchain (genesis only), **When** both methods are called, **Then** `getChainLength` returns 1 and `getChunkCount` returns 1.

---

### Edge Cases

- What happens when disk space runs out during chunk auto-save? The system should log an error and continue operating with the in-memory chain, retrying on the next save opportunity.
- What happens when chunk files exist on disk but are from a different or incompatible serialization version? The system should detect deserialization failure and report the file as corrupted.
- What happens when chunk file numbering has gaps (e.g., chunk 0, chunk 1, chunk 3 — missing chunk 2)? The system should report the missing chunk and load only the contiguous prefix (chunks 0 and 1).
- What happens when the node receives blocks via P2P sync that span multiple new chunks? Each newly filled chunk should be auto-saved as it reaches capacity.
- What happens if a chunk file is locked or has restrictive permissions? The system should log a clear error with the file path and permission details.
- What happens if the periodic save interval fires during a lengthy chunk load at startup? The periodic timer should not start until startup recovery is complete.
- What happens if the operator sets the periodic save interval to zero? The system should treat zero as "disabled" and rely solely on chunk-full and shutdown saves.
- What happens if `replaceChain` is called and the backup directory cannot be created (permissions, disk full)? The system should abort the chain replacement, log the error, and keep the current chain intact.
- What happens if many `replaceChain` calls accumulate backup directories? The system does not auto-purge backups; cleanup is the operator's responsibility.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST automatically save a chunk to disk when it reaches its configured capacity (currently 100 blocks).
- **FR-002**: System MUST save all in-memory chunks (including the current partially-filled chunk) on graceful shutdown (SIGINT/SIGTERM).
- **FR-002a**: System MUST periodically save the current partially-filled chunk to disk at a configurable interval (default: 5 minutes) to bound data loss between full-chunk saves.
- **FR-002b**: System MUST skip a periodic save if the current chunk has not changed since the last save (dirty tracking).
- **FR-002c**: System MUST allow the periodic save interval to be configured by the operator via the `persistence.save_interval_seconds` field in `config.json`, with a default of 300 seconds and a value of zero disabling periodic saves entirely.
- **FR-003**: System MUST discover all existing chunk files from the blockchain data directory on startup, validate the contiguous prefix, and load only the active (latest) chunk into memory. Index loading and rebuild are governed by FR-009.
- **FR-003a**: System MUST load filled chunks from disk on demand when blocks within them are queried, and free them from memory after the query completes.
- **FR-004**: System MUST detect and report corrupted, unreadable, or permission-denied chunk files on startup, identifying the chunk number, file path, and error details in log output.
- **FR-005**: System MUST stop loading at the first corrupted or missing chunk, retaining only the contiguous prefix of valid chunks, and report which chunks were discarded.
- **FR-006**: System MUST validate deserialized block hashes after loading each chunk to detect silent data corruption.
- **FR-007**: System MUST expose a `getChainLength` method on the blockchain interface that returns the total number of blocks across all chunks.
- **FR-008**: System MUST expose a `getChunkCount` method on the blockchain interface that returns the number of chunks in the chain.
- **FR-009**: System MUST load persisted index files (key index, stream registry, stream key index) on startup and trust them as authoritative. If any index file is missing or fails to load, the system MUST rebuild that index by scanning all valid chunk files.
- **FR-010**: System MUST handle chunk auto-save failures (e.g., disk full, permission errors) gracefully by logging the error and continuing operation.
- **FR-011**: *(Subsumed by FR-005.)* System MUST load only a contiguous prefix of chunks when gaps are detected in the chunk file sequence.
- **FR-012**: When `replaceChain` is called, the system MUST move all existing chunk files (and associated index files) into a timestamped backup subdirectory within the blockchain data directory before persisting the replacement chain's chunks.
- **FR-013**: The backup subdirectory MUST be named with a timestamp (e.g., `backups/<ISO-8601-timestamp>`) so multiple replacements do not overwrite each other.

### Key Entities

- **Chunk**: A fixed-capacity container of blocks, serialized as a binary file. Has an index, a block vector, and a file path. May be in-memory, on-disk, or both.
- **Chunk File**: The on-disk serialized representation of a Chunk, named by chunk index and stored in the blockchain data directory.
- **Block**: An individual record in the chain with index, hash, previous hash, entries, and consensus fields. Belongs to exactly one chunk.
- **Blockchain**: The aggregate chain state composed of an ordered sequence of chunks, in-memory indexes, and configuration. Responsible for chunk lifecycle management.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A node with 500+ blocks can be shut down and restarted with zero block loss across all chunks.
- **SC-002**: Chunk auto-save completes within 2 seconds of a chunk reaching capacity (measured on a single-threaded Asio reactor with no other I/O contention).
- **SC-003**: Startup recovery discovers all persisted chunks, loads the active chunk, loads or rebuilds indexes per FR-009, and the node is fully operational (accepting new blocks and queries) within 30 seconds for chains up to 10,000 blocks on commodity hardware (4-core CPU, SSD, 8 GB RAM).
- **SC-004**: Corrupted chunk files are detected and reported in log output on 100% of startup attempts with corrupt data.
- **SC-005**: `getChainLength` and `getChunkCount` return accurate values at all times, including immediately after recovery.
- **SC-006**: No data loss occurs for any filled (auto-saved) chunk, even after an unclean shutdown (SIGKILL).
- **SC-007**: After an unclean shutdown, the maximum number of blocks lost from the current chunk is bounded by one periodic save interval's worth of blocks.

## Assumptions

- The existing Boost.Serialization binary archive format for chunks is stable and does not require migration for this feature.
- Chunk size remains fixed at 100 blocks (configured via `IBlockchain::chunkSize`). Dynamic chunk sizing is out of scope.
- The blockchain data directory has sufficient disk space and write permissions under normal operating conditions.
- Chunk files are stored in the same directory used by the existing `save()`/`load()` implementation on `Chunk`.
- Chunk files follow the existing naming convention `chunk_NNNNNN.dat` (zero-padded 6-digit index). Any change to this convention is out of scope.
- The existing key index, stream registry, and stream key index files are still saved/loaded independently (their persistence is already implemented); this feature adds chunk-level persistence only.
- Re-syncing missing or corrupted chunks from peers is out of scope — the system only detects and reports corruption; recovery via P2P sync is a future feature.
- The default periodic save interval is 5 minutes (300 seconds), configured via `persistence.save_interval_seconds` in `config.json`; this is a reasonable balance between disk I/O overhead and data-loss window for typical low-traffic chains.
- Only the active (current, partially-filled) chunk is kept in memory at runtime. Filled chunks are loaded from disk on demand. Memory optimization (e.g., LRU caching of recently queried chunks) is out of scope for this feature.
- Persisted index files (keys, streams, stream key index) are trusted on startup and not rebuilt unless missing or corrupted. An explicit rebuild command may be added in a future spec.
- The `backups/` subdirectory within the blockchain data directory is used to archive old chunk and index files during chain replacement. Automatic purging of old backups is out of scope; operators manage disk usage manually.
