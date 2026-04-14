# Research: Architecture Remediation

**Feature**: 021-architecture-remediation  
**Date**: 2026-04-13

## R1: Interface Segregation Pattern

**Decision**: Widen `IChainReader` to include all read-only query methods currently on `IBlockchain`. Keep non-virtual `public` inheritance (no diamond exists since `IChainReader` and `IChainWriter` share no common base).

**Rationale**: The current inheritance chain `IBlockchain : IChainReader, IChainWriter` already uses the correct pattern. Moving `getBlockByIndex()`, `getBlocksByKeys()`, `getInclusionProof()`, and `verifyInclusionProof()` from `IBlockchain` to `IChainReader` is a pure header change — method signatures move, implementations stay in `Blockchain.cpp`. No diamond problem exists; `virtual` inheritance is unnecessary.

**Alternatives considered**:
- Virtual inheritance: Unnecessary — no shared base between `IChainReader` and `IChainWriter`.
- Composition over inheritance (e.g. `Blockchain` holds a `ChainReaderImpl`): Over-engineered for the current scope; the interface approach is idiomatic for the codebase.

## R2: Service Layer Design

**Decision**: Introduce `ChainService` as a thin mediator class that owns the validate-append-persist workflow. Network components (`PeerClient`, `BlockPropagation`) call `ChainService::submitBlock()` and `ChainService::submitSyncBatch()` instead of calling `appendBlock()`, `saveChunk()`, `saveKeys()` directly.

**Rationale**: 
- `PeerClient::handle_sync_response()` and `BlockPropagation::appendReceivedBlock()` both duplicate the same pattern: validate → appendBlock → saveChunk(block.index / chunkSize) → saveKeys().
- `PeerServer::handle_blockchain_query()` directly uses `bc.chunkSize` to compute chunk boundaries for sync responses. This couples the wire format to the storage scheme.
- A `ChainService` encapsulates all three steps and owns the chunk-index calculation. Network code only submits blocks; the service handles the rest.

**Constructor design**: `ChainService(IBlockchain &bc)`. Network components take `ChainService&` instead of `IBlockchain&` for mutation operations, plus `const IChainReader&` for read operations.

**Alternatives considered**:
- Making `appendBlock()` handle persistence internally: Would change semantics of `appendBlock()` (currently pure in-memory). Not all callers want auto-persist (e.g. genesis block setup).
- Free functions instead of a class: Loses the ability to inject mock services in tests.

## R3: Exception Hierarchy Design

**Decision**: Introduce a `ChainError` base exception deriving from `std::runtime_error`, with domain-specific subclasses: `PersistenceError`, `ValidationError`, `PeerError`. Convert existing `return bool` failure patterns and log-and-continue patterns to throw appropriate exception types.

**Rationale**:
- The codebase already uses 31 throw statements with `std::runtime_error`, `std::invalid_argument`, and `std::out_of_range`. A domain hierarchy gives callers the ability to catch at the appropriate granularity.
- `PeerManager::add_peer()` and `remove_peer()` return `bool` — these are low-risk to convert since callers already handle the "didn't add" case.
- `saveAllChunks()` log-and-continue is the most critical fix — partial save failures must propagate.

**Exception hierarchy**:
```
std::runtime_error
└── ChainError              (base for all blockchain exceptions)
    ├── ValidationError      (block validation, invalid input)
    ├── PersistenceError     (chunk save/load, key save failures)
    └── PeerError            (peer add/remove/ban failures)
```

**Alternatives considered**:
- `std::error_code` (C++11 approach): More boilerplate, less idiomatic for the codebase.
- Keep mixed patterns: Rejected per audit §6.3 and clarification decision.

## R4: Streaming Chain Replacement

**Decision**: Replace the `replaceChain(const std::vector<Block>&)` signature with a two-phase approach: (1) validate the entire candidate chain via an iterator/callback interface, writing to temporary chunk files, then (2) atomically swap the old chain with the new one. Batch size = chunk size (100 blocks).

**Rationale**:
- Current signature requires O(N) memory for the full candidate chain.
- The chunk-based architecture naturally provides a batch boundary at 100 blocks.
- Two-phase commit (temp files → atomic rename) preserves the original chain on validation failure, satisfying FR-009.
- Difficulty cache is cleared at the start (per clarification Q5), repopulates during validation.

**Streaming protocol change**: `SyncResponse` drops `chunk_index` field. The response simply contains `total_chain_height` and a `blocks` vector. The service layer computes chunk boundaries when persisting.

**Alternatives considered**:
- In-place overwrite with rollback: Risky — a crash during overwrite could corrupt both old and new chain state.
- Memory-mapped files: Would require a new dependency and platform-specific code, violating Constitution §V and §VII.

## R5: Wire Format Changes

**Decision**: Remove `chunk_index` from `SyncResponse` struct. `PeerServer` reads blocks through `IChainReader` and batches them by block-index range (e.g. 100 blocks per response), not by chunk identity. No protocol version negotiation needed.

**Rationale**: Per clarification Q1, all nodes update together. The `chunk_index` field leaked storage internals into the wire format. The replacement is simpler: `SyncResponse` contains `total_chain_height`, `start_index`, and `blocks`.

**Alternatives considered**:
- Keep `chunk_index` as opaque batch ID: Still couples conceptually; cleaner to use block index range.
- Protocol version negotiation: Unnecessary per clarification — small, coordinated node population.

## R6: RpcServer Dependency Narrowing

**Decision**: `RpcServer` constructor takes `const IChainReader&` for read operations plus `IChainWriter&` for `publish()` and `createStream()`. The `SessionHandler` base class is parameterized to accept both narrow interfaces.

**Rationale**: RpcServer calls 11 read methods and only 2 write methods. It never calls persistence methods directly. Narrowing the type signature prevents accidental coupling to persistence or sync methods.

**Alternatives considered**:
- Keep `IBlockchain&` and rely on code review: Doesn't provide compile-time guarantees.
- Separate the RPC server into read-only and write handlers: Over-engineered for 2 write operations.
