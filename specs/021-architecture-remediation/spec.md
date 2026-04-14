# Feature Specification: Architecture Remediation

**Feature Branch**: `021-architecture-remediation`  
**Created**: 2026-04-13  
**Status**: Draft  
**Input**: User description: "Implement section 6 (Architecture Concerns) and 4.5 (replaceChain memory) from AUDIT.md"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Read-Only Consumers See Only Query Methods (Priority: P1)

A developer building a new component that only needs to read chain data (e.g. an analytics exporter or an RPC handler for query endpoints) should depend only on a narrow read interface. They should never see methods like `publish()`, `appendBlock()`, `replaceChain()`, `saveChunk()`, or `saveKeys()` in their API surface.

**Why this priority**: Reducing the interface width is the highest-value architectural change. It directly reduces coupling, makes it easier to reason about what each component can do, and prevents accidental misuse of write operations from read-only contexts.

**Independent Test**: A component depending only on the read interface compiles and functions correctly. Attempting to call a write method through the read interface results in a compile-time error.

**Acceptance Scenarios**:

1. **Given** a component that depends on the read interface, **When** the developer attempts to call a write method (e.g. `publish()`), **Then** the code fails to compile.
2. **Given** the `RpcServer` component, **When** it handles read-only RPC requests (e.g. `getBlockByIndex`, `listStreams`, `getStreamEntries`), **Then** it operates through the read interface without access to persistence or mutation methods beyond the specific write operations it needs (`publish`, `createStream`).
3. **Given** any consumer of the read interface, **When** it queries chain length, block data, stream entries, or inclusion proofs, **Then** all data is accessible without depending on writer or persistence methods.

---

### User Story 2 - Network Layer Submits Blocks Through a Service Boundary (Priority: P2)

The P2P network layer (`PeerClient`, `PeerServer`, `BlockPropagation`) currently calls `IBlockchain` methods directly, coupling block validation, append, and persistence logic to the network code. A developer maintaining the consensus rules or block format should be able to change them without modifying the network layer.

**Why this priority**: Separating domain logic from network transport is essential for maintainability. The current design means that changes to how blocks are validated or stored require touching network code, and vice versa.

**Independent Test**: Network components submit blocks through a service abstraction. Changing how blocks are validated or persisted does not require modifications to `PeerClient`, `PeerServer`, or `BlockPropagation`.

**Acceptance Scenarios**:

1. **Given** a block received over the P2P network, **When** `BlockPropagation` processes it, **Then** it submits the block to the service layer rather than calling `appendBlock()`, `saveChunk()`, and `saveKeys()` directly.
2. **Given** a sync response with new blocks, **When** `PeerClient` processes the response, **Then** it submits the batch to the service layer rather than directly manipulating the chain.
3. **Given** a sync request from a peer, **When** `PeerServer` handles it, **Then** it reads blocks through the read interface without referencing chunk storage details (e.g. `chunkSize`).
4. **Given** the wire format for sync responses, **When** blocks are exchanged, **Then** the protocol does not include `chunk_index` or other storage-specific fields — batching is the service layer's concern.

---

### User Story 3 - Consistent Error Reporting Across All Operations (Priority: P3)

A developer or operator troubleshooting a failure should encounter a predictable error-reporting pattern regardless of which operation failed. Currently, some operations throw exceptions, some return booleans, some log-and-continue, and some silently no-op. The system should adopt a uniform approach so that callers can reliably detect and handle failures.

**Why this priority**: Inconsistent error handling makes it difficult to reason about failure modes, leads to silent data loss (e.g. partial `saveAllChunks` failures being swallowed), and complicates testing. Consistent patterns improve reliability and debuggability.

**Independent Test**: Every mutating operation that can fail communicates failure through a consistent mechanism. No operation silently swallows errors that affect data integrity.

**Acceptance Scenarios**:

1. **Given** a persistence operation that partially fails (e.g. one chunk fails to save), **When** the caller invokes the operation, **Then** the failure is reported through the standard error mechanism rather than being logged and ignored.
2. **Given** a validation operation that detects invalid input, **When** the validation fails, **Then** the caller receives a clear indication of failure rather than a silent no-op.
3. **Given** any mutating operation on the chain, **When** it fails, **Then** the error reporting follows the same pattern used by other mutating operations — callers do not need to check different mechanisms per operation.

---

### User Story 4 - Chain Replacement Operates Within Bounded Memory (Priority: P3)

An operator running a node on a machine with limited RAM should be able to accept a longer valid chain from a peer without the node loading the entire candidate chain into memory at once. Currently `replaceChain()` accepts `const std::vector<Block>&`, requiring the full history in RAM simultaneously.

**Why this priority**: While this is a low-severity performance issue today (chains are short), it becomes critical as the chain grows. Bounding memory usage prevents out-of-memory crashes during chain replacement on resource-constrained nodes.

**Independent Test**: Chain replacement can process a chain that is larger than available RAM by processing blocks in bounded-size batches rather than loading everything at once.

**Acceptance Scenarios**:

1. **Given** a candidate chain of N blocks, **When** `replaceChain` is invoked, **Then** peak memory usage is proportional to the batch size, not to N.
2. **Given** a replacement that fails validation partway through, **When** the failure is detected, **Then** the original chain state is preserved and no partial replacement is committed.
3. **Given** a valid candidate chain, **When** the streaming replacement completes, **Then** the resulting chain state is identical to what the previous all-in-memory approach produced.

---

### Edge Cases

- What happens when the read interface is used after the chain has been replaced? Results should reflect the new chain state.
- What happens when the service layer receives a block that references a chunk boundary? The service layer handles this internally without exposing chunk details to callers.
- What happens when a streaming chain replacement is interrupted by a shutdown signal? The original chain is preserved; partial replacement is rolled back.
- What happens when error handling is standardized but a legacy caller expects the old pattern? Compile-time or test failures catch the mismatch immediately.

### Out of Scope

- Changes to consensus rules or mining algorithms.
- Addition of new RPC endpoints beyond what currently exists.
- Test framework restructuring (e.g. switching test libraries, reorganizing test directory structure).
- New tests are in scope only where needed to cover the new service layer and streaming replacement; existing test structure is not refactored.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The read interface MUST expose all query methods currently used by `RpcServer` for read-only operations: chain length, block count, chunk count, difficulty, block-by-index, blocks-by-keys, stream listing, stream entries, stream entry, inclusion proof generation, and inclusion proof verification. The four query methods currently on `IBlockchain` (`getBlockByIndex`, `getBlocksByKeys`, `getInclusionProof`, `verifyInclusionProof`) MUST be moved to the read interface.
- **FR-002**: The read interface MUST NOT expose any mutation, persistence, or lifecycle methods (`publish`, `appendBlock`, `replaceChain`, `saveChunk`, `saveKeys`, `loadChunk`, `freeChunk`, `setShuttingDown`).
- **FR-003**: `RpcServer` MUST depend on the read interface for all query operations, and on a minimal write interface only for `publish` and `createStream`.
- **FR-004**: A service layer MUST mediate between the network layer and the domain layer, accepting blocks and delegating validation, append, and persistence to the chain.
- **FR-005**: Network components (`PeerClient`, `PeerServer`, `BlockPropagation`) MUST submit blocks and sync batches through the service layer rather than calling chain mutation and persistence methods directly.
- **FR-006**: The sync wire protocol MUST NOT include storage-specific fields such as `chunk_index`; batching strategy MUST be owned by the service layer.
- **FR-007**: All operations that can fail — including chain-domain operations (block validation, persistence, stream operations) and peer management operations (`add_peer`, `remove_peer`, `ban_peer`, `unban_peer`) — MUST report failure through exceptions with domain-specific exception types. No operation may silently swallow errors, return booleans for failure, or log-and-continue when data integrity is at stake.
- **FR-008**: `replaceChain` MUST process the candidate chain in bounded-size batches so that peak memory is proportional to the batch size, not the total chain length.
- **FR-009**: If a streaming chain replacement fails partway through validation, the original chain state MUST be preserved with no partial replacement committed.
- **FR-011**: The difficulty cache MUST be cleared once at the start of a streaming chain replacement (before processing any batches) to prevent stale cache entries from affecting difficulty recalculations during rebuild.
- **FR-010**: The service layer MUST ensure that persistence operations (`saveChunk`, `saveKeys`) are called after successful block acceptance, removing this responsibility from network components.

### Key Entities

- **IChainReader**: Read-only query interface for chain data, including `getBlockByIndex`, `getBlocksByKeys`, `getInclusionProof`, `verifyInclusionProof`, and all existing query methods. Consumers that only need to read depend exclusively on this.
- **IChainWriter**: Mutation interface for chain operations (`publish`, `createStream`, `appendBlock`, `replaceChain`).
- **ChainService**: Mediator between network layer and domain layer. Accepts blocks from the network, validates them, delegates to the chain, and handles persistence.
- **IBlockchain**: Combined interface (inheriting reader + writer) used only by components that genuinely need full access to the chain.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Components that only need read access to the chain compile successfully when depending exclusively on the read interface, with zero access to write or persistence methods.
- **SC-002**: Network components (`PeerClient`, `PeerServer`, `BlockPropagation`) have zero direct calls to chain mutation or persistence methods — all such operations go through the service layer.
- **SC-003**: The sync wire format contains no storage-specific fields (e.g. chunk index).
- **SC-004**: Every mutating operation that can fail uses the same error-reporting mechanism, verifiable by code review.
- **SC-005**: Chain replacement peak memory usage is bounded by batch size regardless of total chain length.
- **SC-006**: All existing tests continue to pass after the refactoring, and new tests cover the service layer and streaming replacement.
- **SC-007**: No regressions in chain sync, block propagation, or RPC functionality after the architectural changes.

## Clarifications

### Session 2026-04-13

- Q: How should backward compatibility be handled when the sync wire format removes `chunk_index`? → A: No backward compatibility needed — all nodes update together; just change the format.
- Q: Should `getBlockByIndex`, `getBlocksByKeys`, `getInclusionProof`, and `verifyInclusionProof` move to `IChainReader`? → A: Yes, move all four to `IChainReader` so the read interface is complete.
- Q: What is the scope of error handling standardization? → A: Full standardization — convert all operations (chain, persistence, peer management) to exceptions.
- Q: Which potential extensions are explicitly out of scope? → A: Consensus rule changes, new RPC endpoints, and test framework restructuring are all out of scope.
- Q: When should the difficulty cache be invalidated during streaming chain replacement? → A: Clear once at the start, before processing any batches.

## Assumptions

- The existing `IChainReader` and `IChainWriter` interfaces provide a starting point and will be extended rather than replaced from scratch.
- The `ChainService` layer is a thin mediator — it does not duplicate validation logic already present in `IBlockchain::isValidNewBlock()`.
- Error handling standardization will adopt exceptions (with domain-specific exception types) as the consistent mechanism across the entire codebase — including chain-domain operations, persistence, and peer management. All return-bool failure patterns and log-and-continue patterns will be converted.
- The streaming `replaceChain` will use chunk-sized batches (matching the existing 100-block chunk size) as the natural batch boundary. The difficulty cache is cleared once at the start of replacement to avoid stale entries; it repopulates naturally during rebuild.
- All nodes in the network are updated together, so the sync wire format change (removing `chunk_index`) does not require protocol version negotiation or backward compatibility support.
- The `IBlockchain` combined interface will continue to exist for components that genuinely need full access (e.g. the `Blockchain` implementation itself). The goal is to narrow what each consumer depends on, not to eliminate the combined interface.
