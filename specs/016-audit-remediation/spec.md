# Feature Specification: Code Audit Remediation

**Feature Branch**: `016-audit-remediation`  
**Created**: 2026-04-12  
**Status**: Draft  
**Input**: User description: "Address bugs, code duplication, performance issues, and thread safety from the code audit report."

## Clarifications

### Session 2026-04-12

- Q: Which threading approach — single-thread enforcement, strand wrappers, or mutexes? → A: Enforce single-threaded `io_context::run()` with runtime assertion + documented contract.
- Q: Difficulty I/O fix strategy — difficulty cache, chunk retention, or both? → A: Both — cache difficulty per adjustment boundary and retain chunks during multi-access operations.
- Q: Should received blocks trust or verify the sender's merkle root? → A: Verify on receipt, then cache the verified value.
- Q: Pending-pool eviction data structure? → A: Combined `std::unordered_map` + `std::deque` for O(1) eviction and lookup.
- Q: Should `recoverChain()` redundant chunk I/O (§4.5) be in scope? → A: Yes, apply chunk retention during recovery validation consistent with FR-004.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Correct Block Count and Consensus Integrity (Priority: P1)

A node operator runs a blockchain node with a chain spanning multiple chunks, some of which have been freed from memory. The node must always report the correct total block count so that consensus validation, difficulty adjustment, and chain-replacement decisions use accurate data.

**Why this priority**: The `getChainBlockCount()` bug (§2.1) causes under-counting of blocks when chunks have been freed. This directly corrupts consensus and difficulty calculations — the highest-severity correctness issue in the audit.

**Independent Test**: Can be tested by building a chain across multiple chunks, freeing older chunks, and verifying `getChainBlockCount()` returns the correct total.

**Acceptance Scenarios**:

1. **Given** a chain spanning 5 chunks where chunks 0–3 have been freed from memory, **When** `getChainBlockCount()` is called, **Then** the returned count matches the actual total number of blocks across all chunks.
2. **Given** a node receiving a new block after chunks have been freed, **When** difficulty adjustment runs, **Then** the adjustment uses the correct block count and produces the expected difficulty.
3. **Given** a chain-replacement candidate arrives from a peer, **When** the node compares chain lengths, **Then** the comparison uses accurate block counts for both chains.

---

### User Story 2 - Thread-Safe Concurrent Operation (Priority: P1)

A node operator runs a node that handles simultaneous block arrivals from multiple peers, RPC requests, and periodic save operations. The node must not corrupt internal state or crash due to data races.

**Why this priority**: The threading model is undocumented and shared mutable state in `BlockPropagation`, `PeerManager`, `Blockchain`, and `RpcServer` is accessed from async callbacks without synchronization, risking data corruption under concurrent load.

**Independent Test**: Can be tested by verifying that either single-threaded execution is enforced with documentation, or that strand/mutex protections are in place and concurrent operations complete without corruption.

**Acceptance Scenarios**:

1. **Given** a node processing blocks from multiple peers concurrently, **When** two blocks arrive simultaneously, **Then** no data corruption occurs in dedup sets, pending pools, or rate limiters.
2. **Given** a periodic save timer fires while a new block is being appended, **When** both operations access the chain state, **Then** no race condition occurs and data remains consistent.
3. **Given** multiple RPC requests arrive on the same connection, **When** they are processed, **Then** no buffer corruption occurs.
4. **Given** peer exchange and disconnect callbacks fire concurrently, **When** both access the peer list, **Then** the peer state remains consistent.

---

### User Story 3 - Efficient Difficulty Calculation (Priority: P2)

A node operator runs a chain with thousands of blocks. When a new block is mined or received, difficulty adjustment must complete promptly without excessive disk I/O from repeatedly loading and freeing the same chunks.

**Why this priority**: `getDifficultyForHeight()` triggers O(W×D) chunk load/free cycles (§4.2), and `getBlockByIndex()` immediately frees chunks it just loaded (§2.4). On chains with 100,000+ blocks, this causes severe I/O overhead and slowdowns.

**Independent Test**: Can be tested by building a long chain, calling difficulty calculation, and measuring that chunk load/free operations are bounded rather than proportional to chain length times adjustment windows.

**Acceptance Scenarios**:

1. **Given** a chain of 10,000 blocks with an adjustment window of 10, **When** difficulty is recalculated, **Then** the same chunk is not loaded from disk more than once during the calculation.
2. **Given** repeated calls to `getBlockByIndex()` for indices within the same chunk, **When** the first call loads the chunk, **Then** the chunk remains available for subsequent calls within the same operation.
3. **Given** a new block arrives requiring difficulty adjustment, **When** the adjustment completes, **Then** it does so without loading every historical chunk from disk.

---

### User Story 4 - Reduced Code Duplication (Priority: P2)

A developer maintaining the codebase needs to make a change that affects chunk filenames, RPC error responses, peer broadcasting, or test setup. The change should be made in one place and take effect everywhere, rather than requiring updates to multiple near-identical code sites.

**Why this priority**: Six duplication clusters were identified in the audit (§3). Duplicated code increases maintenance burden and risk of inconsistent fixes. Addressing duplication makes subsequent features and bug fixes safer and faster.

**Independent Test**: Can be tested by verifying that duplicated patterns have been extracted into shared helpers or utilities, and that all call sites use the shared implementation.

**Acceptance Scenarios**:

1. **Given** the chunk filename pattern is used in multiple functions, **When** the filename format needs to change, **Then** it can be changed in a single location.
2. **Given** RPC error responses are constructed in multiple handlers, **When** the JSON-RPC response format changes, **Then** only the shared helper needs updating.
3. **Given** `broadcast_block()` and `relay_block()` contain near-identical peer-sending loops, **When** the sending logic changes, **Then** it is maintained in one place.
4. **Given** test setup boilerplate (temp directories, test configs, mine helpers) is used across test files, **When** the setup pattern changes, **Then** all tests pick up the change from a single shared location.

---

### User Story 5 - Correct Merkle Root and Block Handling (Priority: P3)

When a node receives blocks from peers during chain synchronization, it should not redundantly recompute merkle roots and hashes for blocks that already carry valid values from the sending node.

**Why this priority**: This is a correctness and performance issue (§2.2) — wasted CPU proportional to entries per block — but does not produce incorrect results, only redundant work.

**Independent Test**: Can be tested by syncing blocks from a peer and verifying that the received merkle root and hash are preserved rather than recomputed.

**Acceptance Scenarios**:

1. **Given** a block is received from a peer with a valid merkle root, **When** the block is constructed on the receiving node, **Then** the merkle root from the sender is used without recomputation.
2. **Given** a block is created locally with new entries, **When** the block is constructed, **Then** the merkle root is computed fresh from the entries.

---

### User Story 6 - Robust IPv6 Peer Identification (Priority: P3)

A node operator runs nodes on an IPv6 network. Peer identification strings that contain IPv6 addresses must be parsed correctly without splitting at the wrong colon.

**Why this priority**: The `sender_key` parsing bug (§2.3) is a latent issue on IPv6 networks. While most current deployments use IPv4, this must be fixed to ensure correctness across all network configurations.

**Independent Test**: Can be tested by constructing sender keys with IPv6 addresses and verifying that host and port are extracted correctly.

**Acceptance Scenarios**:

1. **Given** a sender key of `[::1]:8333`, **When** the host and port are parsed, **Then** host is `::1` and port is `8333`.
2. **Given** a sender key of `192.168.1.1:8333`, **When** the host and port are parsed, **Then** host is `192.168.1.1` and port is `8333`.

---

### Edge Cases

- What happens when `getChainBlockCount()` is called on an empty chain with zero chunks?
- How does the system behave when all chunks have been freed from memory and a full count is requested?
- What happens when a chunk file is corrupted and `recoverChain()` validates the same chunk multiple times?
- How does the system handle a peer connection dropping mid-block-propagation when thread safety is enforced?
- What happens when the pending pool eviction runs concurrently with new block insertion?
- How does `sender_key` parsing handle malformed strings (empty string, missing port, invalid port number)?
- What happens when `dirty_` flag is checked simultaneously by the save timer and an append operation?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `getChainBlockCount()` MUST return the cached `totalBlockCount_` value instead of iterating over chunks, ensuring correctness even when chunks are freed from memory.
- **FR-002**: The threading model MUST enforce single-threaded `io_context::run()` execution via a runtime assertion and a documented contract. The assertion MUST verify that only one thread calls `io_context::run()`. The threading model MUST be documented in a code comment at the `io_context::run()` call site.
- **FR-003**: `getDifficultyForHeight()` MUST cache difficulty values per adjustment boundary so that subsequent difficulty calculations do not re-traverse historical chunks. The cache MUST be held in-memory (rebuilt lazily on startup) and invalidated on chain replacement.
- **FR-004**: `getBlockByIndex()` MUST retain loaded chunks in memory for the duration of a multi-access operation (e.g., difficulty calculation, chain validation, chain recovery) rather than immediately freeing them after a single read. Chunks MUST only be freed after the calling operation completes. This applies to `getDifficultyForHeight()`, `recoverChain()` validation, and any other path that accesses multiple blocks across chunks in sequence.
- **FR-005**: A shared chunk-filename utility MUST be used by all code sites that construct chunk filenames, replacing the 8 duplicated inline constructions.
- **FR-006**: RPC error-response construction MUST use a shared helper function rather than repeating the JSON-RPC boilerplate in each of the 9 handler methods.
- **FR-007**: `broadcast_block()` and `relay_block()` MUST share a common peer-sending implementation, differing only by an optional exclusion parameter.
- **FR-008**: `sender_key` parsing MUST correctly handle both IPv4 and IPv6 address formats, using the last colon (or bracket-aware parsing) to split host from port.
- **FR-009**: Block construction for received (synced) blocks MUST verify the sender's merkle root against the block's entries on receipt. Once verified, the merkle root MUST be cached and not recomputed again. If verification fails, the block MUST be rejected.
- **FR-010**: A shared test-helpers module MUST provide reusable temp-directory setup, `ConsensusConfig` defaults, `mineTestBlock()`, and valid-chain building utilities for all test files.
- **FR-011**: The `dirty_` flag MUST NOT be prematurely cleared during chunk rotation in `publish()` — the flag must remain `true` until the new block is safely appended.
- **FR-012**: `getStreamEntries()` with-key and without-key branches MUST share a common block-lookup implementation, differing only in the filter predicate.
- **FR-013**: Pending-pool eviction in `BlockPropagation` MUST use a combined `std::unordered_map` + `std::deque` to achieve O(1) eviction of the oldest entry and O(1) lookup by block hash, replacing the current linear scan.

### Key Entities

- **Block**: Unit of chain data containing index, timestamp, previous hash, entries, nonce, difficulty, merkle root, and hash.
- **Chunk**: Fixed-size container of blocks persisted to disk as a binary archive, identifiable by a zero-padded sequential filename.
- **PeerEntry**: Network peer identified by host and port, with connection state and error tracking.
- **BlockPropagation State**: Deduplication set, pending pool, rate limiters, and sync queue used during block relay.
- **ThreadingModel**: The documented contract for how async callbacks and shared state are synchronized.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Block count reported by the node matches the actual number of blocks in the chain under all conditions, including when chunks have been freed from memory.
- **SC-002**: No data races are detected when the node processes concurrent operations (block arrivals, RPC requests, periodic saves, peer exchanges).
- **SC-003**: Difficulty calculation for a chain of 10,000 blocks completes without loading the same chunk from disk more than once per calculation.
- **SC-004**: All 6 chunk-filename construction sites use a single shared utility function.
- **SC-005**: All 9 RPC error-response methods are replaced by calls to a single shared response builder.
- **SC-006**: Peer sending logic exists in one shared function, called by both broadcast and relay paths.
- **SC-007**: IPv6 sender keys (e.g., `[::1]:8333`) are parsed correctly without misidentifying the port.
- **SC-008**: Test setup boilerplate is reduced by consolidation into shared test helpers, with all test files using the shared module.
- **SC-009**: All existing tests continue to pass after remediation changes.
- **SC-010**: Received blocks during sync preserve the sender's merkle root and hash rather than recomputing them.

## Assumptions

- The current deployment runs `io_context` on a single thread. The threading fix will formalize this constraint with a runtime assertion and documented contract. Multi-threaded synchronization (strands/mutexes) is explicitly out of scope for this feature.
- All bugs, duplication, and performance issues addressed are those documented in the Code Audit Report (docs/AUDIT.md, dated 2026-04-12). Any issues discovered after this spec is written are out of scope.
- The existing test suite provides sufficient coverage to validate that remediation changes do not introduce regressions. New tests will be added for the specific bugs being fixed.
- The `replaceChain()` memory concern (§4.8 — loading entire candidate chain into RAM) is out of scope for this feature, as it requires a larger architectural change to streaming chain replacement.
- The architecture concerns (§6.2–6.4: splitting `Blockchain.cpp`, introducing service layers, narrowing `IBlockchain`) are out of scope for this feature, as they are structural refactors rather than bug fixes or targeted improvements.
- The O(n) peer lookup optimization (§4.3) will use the existing `std::vector` with improved algorithms rather than a full data-structure change, unless profiling shows it is a bottleneck at the configured 256-peer maximum.
