# Feature Specification: Graceful Multi-Chunk Shutdown & Startup

**Feature Branch**: `011-graceful-lifecycle`  
**Created**: 2026-04-11  
**Status**: Draft  
**Input**: User description: "Implement 011 — Graceful Multi-Chunk Shutdown & Startup. Signal handler saves only chunk 0 and keys. Multi-chunk chains lose unsaved data on shutdown. Startup loads only chunk 0. Fix the lifecycle to persist and restore the full chain state."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Complete Data Preservation on Shutdown (Priority: P1)

As a node operator, when I stop the blockchain daemon (via SIGINT or SIGTERM), all in-memory chain data — including every modified chunk, index files, and stream metadata — is persisted to disk so that no blocks or transactions are lost.

**Why this priority**: Data loss on shutdown is the most critical defect. Operators who have accumulated blocks across multiple chunks will lose all modifications except the last chunk, silently corrupting the persisted state.

**Independent Test**: Stop a running node that has blocks spanning multiple chunks. Restart the node and verify every block that existed before shutdown is still present and queryable.

**Acceptance Scenarios**:

1. **Given** a node with blocks spanning 3 chunks where chunks 0 and 1 have been modified since last save, **When** the operator sends SIGTERM, **Then** all 3 chunk files on disk reflect the current in-memory state.
2. **Given** a node with unsaved changes in multiple chunks, **When** the operator sends SIGINT, **Then** the shutdown process saves every dirty chunk before the process exits.
3. **Given** a node where only the last chunk has changes, **When** shutdown occurs, **Then** only the last chunk is written (no unnecessary I/O for clean chunks).
4. **Given** a chunk save fails during shutdown (e.g., disk full), **When** the error occurs, **Then** the node logs the failure for each affected chunk and continues saving remaining chunks rather than aborting.
5. **Given** a block propagation message arrives after the shutdown signal, **When** the shutdown sequence has begun, **Then** the block is rejected and not added to any chunk before the save phase runs.

---

### User Story 2 - Full Chain Restore on Startup (Priority: P1)

As a node operator, when I start the blockchain daemon against a data directory containing multiple chunk files, the node loads all valid chunks into its chain state and is immediately ready to serve queries and accept new blocks for the full chain history.

**Why this priority**: Equally critical to shutdown — if a node cannot restore its full chain on startup, multi-chunk persistence is effectively broken despite data being on disk.

**Independent Test**: Start a node against a data directory with 5 chunk files. Verify the node reports the correct total block count and can serve blocks from any chunk (historical chunks loaded on demand).

**Acceptance Scenarios**:

1. **Given** a data directory with chunk files 0 through 4, **When** the node starts, **Then** it loads all 5 chunks and reports the correct total block count.
2. **Given** a data directory with chunk files 0 through 4 where chunk 3 is corrupted, **When** the node starts, **Then** it loads chunks 0–2, logs a warning about chunk 3, and operates with the valid prefix.
3. **Given** an empty data directory with no chunk files, **When** the node starts, **Then** it creates a fresh chain with a genesis block (existing behavior preserved).
4. **Given** a data directory with chunks that have a cross-chunk linkage break between chunk 2 and chunk 3, **When** the node starts, **Then** it loads only chunks 0–2 and logs the linkage error.

---

### User Story 3 - Per-Chunk Dirty Tracking (Priority: P2)

As a node operator, I want the system to track which chunks have been modified since their last save so that shutdown and periodic saves only write chunks that actually changed, minimizing disk I/O and save duration.

**Why this priority**: Performance optimization that prevents unnecessary writes. Without dirty tracking, the system must either save everything (slow for large chains) or guess which chunks changed (error-prone).

**Independent Test**: Modify blocks in chunk 1 only. Trigger a save and verify that only chunk 1 is written to disk, not chunks 0 or 2.

**Acceptance Scenarios**:

1. **Given** a node with 3 chunks where only chunk 1 has been modified, **When** a save operation runs, **Then** only chunk 1 is written to disk.
2. **Given** a node where a chain replacement modifies blocks in chunks 0 and 1, **When** the next save occurs, **Then** both chunk 0 and chunk 1 are persisted.
3. **Given** a newly started node that has not received any blocks, **When** a periodic save triggers, **Then** no chunk files are written.

---

### User Story 4 - Startup Integrity Verification (Priority: P2)

As a node operator, I want the node to verify the integrity of all chunk files on startup — checking both internal consistency and cross-chunk linkage — so that I can trust the chain state is valid before the node begins serving requests.

**Why this priority**: Without integrity checks, a corrupted chunk file could silently introduce invalid data into the chain, undermining trust in the node's state.

**Independent Test**: Corrupt a byte in a chunk file on disk, start the node, and verify it detects and reports the corruption.

**Acceptance Scenarios**:

1. **Given** all chunk files are valid and properly linked, **When** the node starts, **Then** it logs successful integrity verification and loads the full chain.
2. **Given** chunk file 2 of 5 has a corrupted archive, **When** the node starts, **Then** it logs the corruption, loads only chunks 0–1, and reports a reduced chain length.
3. **Given** chunk 3 has valid data but its first block's previous hash does not match chunk 2's last block hash, **When** the node starts, **Then** it stops loading at chunk 2 and logs the linkage error.
4. **Given** the node recovers a partial chain due to corruption, **When** the operator queries chain status, **Then** the reported block count and chunk count reflect only the valid prefix.
5. **Given** `fast_startup` is enabled in config, **When** the node starts, **Then** chunk integrity validation is skipped and all sequentially discovered chunks are loaded without verification.

### Edge Cases

- What happens when the data directory becomes read-only between startup and shutdown? The node should log an error for each chunk it cannot save and continue attempting the remaining chunks.
- How does the system handle a chunk file that exists on disk but is zero bytes? It should be treated as corrupted and excluded from the valid chain prefix.
- What happens if a new chunk is created and immediately followed by shutdown before any blocks are added? The empty chunk is not saved to disk. On next startup, sequential chunk discovery will not find a gap, and the in-memory chunk will be recreated as needed.
- How does startup behave when index files (keys.dat, streams.dat, stream_index.dat) are missing but chunk files are intact? The system should rebuild indexes from the chunk data.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: On receiving SIGINT or SIGTERM, the system MUST first stop accepting new blocks (freeze block ingestion), then iterate all in-memory chunks and save every chunk that has been modified since its last save.
- **FR-002**: The system MUST track a dirty flag per chunk that is set when blocks are added to or modified within that chunk, and cleared when the chunk is successfully saved. When a historical chunk is modified (e.g., via chain replacement), it MUST be reloaded into memory, marked dirty, and kept resident until the next save (periodic or shutdown).
- **FR-003**: On startup, the system MUST discover all chunk files in the data directory by scanning for sequentially numbered files (chunk_000000.dat, chunk_000001.dat, …).
- **FR-004**: On startup, the system MUST validate each discovered chunk file by loading it and verifying its internal consistency (deserialization success, non-empty content for non-final chunks). After validation, only the active (last) chunk remains resident in memory; historical chunks are represented by lightweight placeholders and loaded on demand when accessed.
- **FR-005**: On startup, the system MUST verify cross-chunk linkage by confirming that the first block of chunk N+1 references the hash of the last block of chunk N.
- **FR-006**: On startup, if a chunk fails validation or linkage, the system MUST stop loading at the last valid chunk, log the error, and operate with the valid chain prefix.
- **FR-007**: The periodic save mechanism MUST save all dirty chunks, not just the active (last) chunk.
- **FR-008**: If saving a chunk fails during shutdown, the system MUST log the error and continue attempting to save remaining chunks.
- **FR-011**: The system MUST NOT save empty chunks (zero blocks) to disk. Empty chunks are skipped during both periodic and shutdown save operations.
- **FR-009**: On startup with no chunk files present, the system MUST create a fresh chain with a genesis block (preserving existing behavior).
- **FR-010**: On startup, if index files are missing but chunk files are valid, the system MUST rebuild key, stream, and stream-key indexes from the chunk data.
- **FR-012**: The system MUST support a `fast_startup` configuration option (default: off). When enabled, chunk integrity validation and cross-chunk linkage checks are skipped on startup, and chunks are loaded based on sequential file discovery alone.

### Key Entities

- **Chunk**: A fixed-capacity container of blocks, persisted as a binary file. Each chunk now carries a dirty flag indicating unsaved modifications.
- **Dirty Flag**: A per-chunk boolean indicating whether the chunk has been modified since its last successful save to disk.
- **Chain Prefix**: The longest contiguous sequence of valid, correctly linked chunks starting from chunk 0. The node operates on this prefix even if later chunk files exist but are invalid.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After a shutdown/restart cycle with blocks spanning multiple chunks, 100% of blocks present before shutdown are recoverable and queryable after restart.
- **SC-002**: Shutdown save operations write only chunks that have been modified, reducing unnecessary disk writes to zero for clean chunks.
- **SC-003**: Startup detects and reports any corrupted or unlinked chunk files, loading the longest valid prefix without crashing.
- **SC-004**: A node with 10 chunk files completes startup integrity verification and is ready to serve requests within a reasonable time relative to chain size. With `fast_startup` enabled, startup skips validation and loads the chain immediately from sequential discovery.
- **SC-005**: No data is silently lost or corrupted during normal shutdown/startup cycles under any supported operating conditions.

## Clarifications

### Session 2026-04-11

- Q: Should startup load all chunk data into memory or use placeholders with on-demand loading? → A: Placeholder + active-chunk-only; historical chunks loaded on demand.
- Q: When a chain replacement modifies historical chunks, should they save immediately or stay resident until next save? → A: Mark dirty and keep resident until next periodic or shutdown save.
- Q: Should empty chunks (zero blocks) be saved to disk? → A: No; empty chunks are skipped during save to avoid ambiguity on startup.
- Q: Should shutdown stop accepting new blocks before saving? → A: Yes; an explicit "stop accepting blocks" phase precedes the save sequence to prevent inconsistent state.
- Q: Should startup always validate all chunks, or allow skipping validation? → A: Validate by default; provide a `fast_startup` config option (default: off) to skip chunk validation for trusted environments.

## Assumptions

- Chunk files use the existing Boost.Serialization binary archive format and naming convention (chunk_NNNNNN.dat).
- The existing `recoverChain` and `discoverChunks` infrastructure provides a foundation that can be extended rather than replaced.
- The filesystem is reliable during normal operation; the dirty-tracking optimization does not need to guard against bit-rot or silent filesystem corruption between saves.
- Periodic save intervals remain configurable via `config.json` (existing `save_interval_seconds` setting).
- The maximum number of chunks a node will manage is bounded by available disk space, not by an artificial limit in the software.
