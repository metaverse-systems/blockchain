# Feature Specification: Audit Bug & Security Fixes

**Feature Branch**: `018-audit-bug-security-fixes`  
**Created**: 2026-04-13  
**Status**: Draft  
**Input**: User description: "Fix the bugs in section 2 and section 3 of the code audit report"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Chain Sync Completes Successfully (Priority: P1)

An operator runs two blockchain nodes and initiates chain synchronization. When node B requests blocks from node A, responses arrive in chunk-sized batches. The received blocks are validated and appended to node B's local chain. After sync completes, both nodes hold the same chain height and identical block data.

**Why this priority**: Sync is the most fundamental multi-node operation. Without it, no node can catch up to the network — the blockchain is effectively single-node only. This is a complete functional break.

**Independent Test**: Start two nodes, mine blocks on one, trigger sync on the other, and verify the second node's chain height increases to match.

**Acceptance Scenarios**:

1. **Given** node B has 0 blocks and node A has 50 blocks across 5 chunks, **When** node B syncs from node A, **Then** node B's local chain height reaches 50 and all block hashes match node A's chain.
2. **Given** node B already has 30 blocks and node A has 50 blocks, **When** node B syncs from node A, **Then** only blocks 31–50 are appended and existing blocks 1–30 remain unchanged.
3. **Given** node A sends a batch containing blocks with invalid hashes, **When** node B processes the sync response, **Then** the invalid blocks are rejected and node B's chain remains at its previous height.
4. **Given** node B has 30 blocks and node A sends a batch containing blocks 20–40 where block 25's hash differs from node B's local block 25, **When** node B processes the sync response, **Then** the entire batch is rejected, an error is logged, and node B's chain remains at height 30.
5. **Given** node A sends a sync response with zero blocks in a batch, **When** node B processes the response, **Then** node B logs a warning, stops requesting further batches from node A, and retains its current chain height.

---

### User Story 2 - RPC Block Query Returns Error for Invalid Index (Priority: P1)

An RPC client queries a block by index. If the requested index is within the chain, the block data is returned. If the index exceeds the chain length, the system returns a structured error response instead of crashing.

**Why this priority**: An unauthenticated RPC client can crash the node by requesting an out-of-range block index. This is both a security vulnerability and an availability issue.

**Independent Test**: Start a node with a known chain length, send a `getBlockByIndex` RPC request with an index beyond the chain, and verify a JSON-RPC error response is returned.

**Acceptance Scenarios**:

1. **Given** a node with 10 blocks (indices 0–9), **When** an RPC client requests block at index 10, **Then** the system returns a JSON-RPC error with code -32001 and message "Block not found".
2. **Given** a node with 10 blocks, **When** an RPC client requests block at index 5, **Then** the block data is returned successfully.
3. **Given** a node with 0 blocks, **When** an RPC client requests any block index, **Then** the system returns a JSON-RPC error with code -32001 and message "Block not found".

---

### User Story 3 - Node Starts with Malformed Seed Node Arguments (Priority: P1)

An operator starts a node with `--seed-node` arguments. If a seed node address contains a non-numeric port or an out-of-range port, the system reports a clear error message and exits gracefully instead of crashing with an uncaught exception.

**Why this priority**: User-supplied CLI input that crashes the process is a security issue. Operators need clear feedback when configuration is wrong.

**Independent Test**: Start the node binary with `--seed-node host:abc` and verify it prints an error and exits with a non-zero status code.

**Acceptance Scenarios**:

1. **Given** a node is started with `--seed-node 192.168.1.1:abc`, **When** the node parses startup arguments, **Then** it prints an error message indicating the port is invalid and exits with a non-zero code.
2. **Given** a node is started with `--seed-node 192.168.1.1:99999`, **When** the node parses startup arguments, **Then** it prints an error message indicating the port is out of range and exits with a non-zero code.
3. **Given** a node is started with `--seed-node 192.168.1.1:8333`, **When** the node parses startup arguments, **Then** the seed node is accepted and the node starts normally.
4. **Given** a node is started with `--seed-node just-a-hostname`, **When** the node parses startup arguments, **Then** it prints an error message indicating the expected host:port format and exits with a non-zero code.

---

### User Story 4 - Chain Recovery Performs Efficiently (Priority: P2)

An operator restarts a node that has accumulated many chunk files on disk. During chain recovery, each chunk file is read from disk only once, regardless of whether fast-startup mode is enabled. Recovery startup time is proportional to chunk count, not a multiple of it.

**Why this priority**: Loading each chunk up to 3 times triples startup time for nodes with large chain histories. This degrades operator experience but does not break correctness.

**Independent Test**: Restart a node with 100+ chunk files, measure recovery time, and confirm each chunk file is opened at most once during recovery.

**Acceptance Scenarios**:

1. **Given** a node with 100 chunk files, **When** recovery runs in non-fast-startup mode, **Then** each chunk file is deserialized from disk at most once.
2. **Given** a node with 100 chunk files, **When** recovery validates cross-chunk linkage between chunk N-1 and chunk N, **Then** chunk N-1 data is reused from the previous iteration rather than reloaded.

---

### User Story 5 - Peer Key Parsing Validates Port Range (Priority: P2)

When the system parses peer addresses from configuration or network messages, port values outside the valid range (1–65535) are rejected with a clear error. Non-numeric or overflowing port strings produce descriptive error messages instead of silent truncation or uncaught exceptions.

**Why this priority**: Silent truncation of port values can cause connections to the wrong port. This is a correctness issue that could cause subtle networking bugs.

**Independent Test**: Call the peer key parser with port values of 0, -1, 70000, and "abc", and verify each produces a descriptive error.

**Acceptance Scenarios**:

1. **Given** a peer key string `192.168.1.1:0`, **When** the system parses the peer key, **Then** it rejects the key with an error indicating the port is out of range.
2. **Given** a peer key string `192.168.1.1:70000`, **When** the system parses the peer key, **Then** it rejects the key with an error indicating the port is out of range.
3. **Given** a peer key string `192.168.1.1:abc`, **When** the system parses the peer key, **Then** it rejects the key with an error indicating the port is not a valid number.
4. **Given** a peer key string `[::1]:8333`, **When** the system parses the peer key, **Then** it is accepted and correctly resolved to host `::1` and port `8333`.

---

### User Story 6 - getBlockByIndex Internal Resize Uses Correct IDs (Priority: P3)

When the system internally resizes its chunk vector to accommodate a block lookup, each intermediate chunk entry is initialized with the correct chunk ID corresponding to its position. No chunk entries share incorrect IDs.

**Why this priority**: The current bug has no runtime effect because entries are overwritten on load, but it violates correctness invariants and could cause issues if future code relies on chunk IDs before loading.

**Independent Test**: Request a block at an index that requires resizing the chunk vector by multiple entries, and verify each intermediate chunk entry has the correct ID.

**Acceptance Scenarios**:

1. **Given** the internal chunk vector has 2 entries, **When** a block in chunk 5 is requested, **Then** the vector is resized and each new entry at positions 2, 3, 4, and 5 has a chunk ID matching its position index.

---

### Edge Cases

- What happens when a sync response contains zero blocks in a batch?
- What happens when `getBlockByIndex` is called with the maximum possible `size_t` value?
- What happens when a seed node argument contains no colon separator at all?
- What happens when a peer key contains an IPv6 address with a port of exactly 65535?
- What happens when chunk recovery encounters a chunk file that exists on disk but is empty or corrupt?

## Clarifications

### Session 2026-04-13

- Q: What should happen when overlapping blocks during sync have mismatched hashes? → A: Abort the sync for that batch and log an error; do not append any blocks from the mismatched batch.
- Q: What should happen when a sync response contains zero blocks in a batch? → A: Treat as end-of-sync: log a warning and stop requesting further batches from that peer.
- Q: What should happen when a seed node argument has no colon separator? → A: Reject with an error message showing expected host:port format and exit.
- Q: Should the spec explicitly declare audit §4–§7 out of scope? → A: Yes, add an explicit out-of-scope section listing audit §4–§7.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The sync handler MUST append validated blocks from a sync response to the local chain for all blocks at or above the local chain height. If any overlapping block (below local height) has a hash mismatch with the local chain, the handler MUST abort the sync for that batch, log an error, and append no blocks. If a sync response contains zero blocks, the handler MUST treat it as end-of-sync, log a warning, and stop requesting further batches from that peer.
- **FR-002**: The `getBlockByIndex` RPC handler MUST check the requested index against the current chain length and return a JSON-RPC error (code -32001, message "Block not found") if the index is out of range.
- **FR-003**: Seed node port parsing from CLI arguments MUST catch parsing exceptions and validate that the port is within the range [1, 65535], exiting with a descriptive error message on failure. If the seed node string contains no colon separator, the system MUST reject it with an error message showing the expected host:port format and exit.
- **FR-004**: The `parsePeerKey()` utility MUST validate that the parsed port integer is within [1, 65535] and throw a descriptive `std::invalid_argument` for values outside that range or non-numeric strings.
- **FR-005**: Chain recovery MUST NOT load the same chunk file more than once during a single recovery pass; chunk data loaded for validation MUST be reused for cross-chunk linkage checks and block counting.
- **FR-006**: When the chunk vector is resized during `getBlockByIndex`, each new entry MUST be initialized with a chunk ID equal to its vector position index.

### Key Entities

- **Block**: The fundamental unit of the chain; identified by its index and hash.
- **Chunk**: A fixed-size group of contiguous blocks stored together on disk; identified by a numeric chunk ID.
- **PeerAddress**: A network endpoint represented by a host string and a 16-bit port number in the range [1, 65535].
- **SyncResponse**: A message containing a chunk index, a batch of blocks, and the total chain height of the sending peer.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: After syncing from a peer, the local chain height matches the peer's advertised chain height (for a valid chain).
- **SC-002**: 100% of `getBlockByIndex` RPC requests with out-of-range indices return a structured error response; zero such requests cause a process crash.
- **SC-003**: 100% of invalid seed node port values (non-numeric, negative, zero, or >65535) produce a user-readable error message and a non-zero exit code; zero such values cause an uncaught exception crash.
- **SC-004**: Node recovery time with N chunk files is proportional to N (single-pass), not 3N (triple-load).
- **SC-005**: All peer key port values outside [1, 65535] are rejected with a descriptive error before being used in network operations.
- **SC-006**: All six issues identified in audit sections 2 and 3 are resolved and verified by targeted tests.

## Assumptions

### Out of Scope

The following audit report sections are explicitly **not addressed** by this feature and are deferred to future work:

- **§4 Performance Issues** (O(n) peer lookups, RPC dispatch chain, string construction in log calls, `replaceChain()` memory usage)
- **§5 Code Duplication** (packet serialization duplication, test helper duplication)
- **§6 Architecture Concerns** (wide `IBlockchain` interface, domain/network separation, inconsistent error handling patterns). Note: the sync protocol currently leaks storage details (`chunk_index` in `SyncResponse`). This should be addressed alongside the §6.2 ChainService introduction — the network layer should exchange blocks only, with the service layer managing chunk-based batching internally.
- **§7 Test Quality** (trivial assertions, vacuous tests, timing-dependent integration tests, coverage gaps unrelated to §2/§3 fixes)

Note: The §4.3 performance issue (`recoverChain()` loads chunks multiple times) **is** in scope because it is the same underlying bug as §2.2.

### Assumptions

- The existing `appendBlock()` or equivalent method on the blockchain object is available and suitable for adding individual sync'd blocks to the chain.
- The `getBlockRange` RPC handler's existing bounds-checking pattern is the correct model for the `getBlockByIndex` fix.
- The `ChunkHandler` class supports being returned or cached by value from `validateChunk()` without excessive overhead.
- The `parsePeerKey()` function is the canonical port-parsing utility and all new port-parsing code should follow the same pattern.
- Fixing the `getBlockByIndex` resize bug (§2.3) is low risk since the current behavior has no observable runtime effect.
- Existing test infrastructure (Catch2, `TestHelpers.hpp`) is sufficient for writing targeted tests for each fix.
