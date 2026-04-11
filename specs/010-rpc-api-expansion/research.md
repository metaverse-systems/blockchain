# Research: RPC API Expansion

**Feature**: 010-rpc-api-expansion  
**Date**: 2026-04-11

## R1: IBlockchain Interface Gap for getCurrentDifficulty

**Decision**: Add `virtual uint32_t getCurrentDifficulty() const = 0` to `IBlockchain`.

**Rationale**: `getNodeStatus` needs the current mining difficulty. `getCurrentDifficulty()` exists on the concrete `Blockchain<ChunkHandler>` class but not on the `IBlockchain` interface. The RPC server holds an `IBlockchain &` reference, so the method must be on the interface. This is a single-line addition with no functional change — the concrete class already implements the method, it just needs the `override` keyword added.

**Alternatives considered**:
- `dynamic_cast<Blockchain<Chunk>*>(&bc)` — fragile, violates interface abstraction, breaks if a different IBlockchain implementation is used.
- Pass `ConsensusConfig` separately to RPC server — adds constructor complexity for a single field; difficulty is dynamic (changes with chain height), not static config.

## R2: RPC Method Dispatch Pattern

**Decision**: Continue the existing if-else chain pattern in `RpcServer::do_read()`.

**Rationale**: All 16+ existing methods use sequential if-else dispatch on `object["method"]`. Converting to a map-based dispatch would be a refactor outside the scope of this feature. The 4 new methods add negligible overhead to the chain (string comparisons are very fast for this count).

**Alternatives considered**:
- `std::unordered_map<std::string, std::function<...>>` — Cleaner architecture, but would be a separate refactoring spec affecting all existing methods. Not justified for 4 additions.

## R3: getBlockRange Maximum Range Size

**Decision**: Hard-coded constant of 1000 blocks. Error code `-32602` when exceeded.

**Rationale**: With blocks containing up to 128 MB per entry, a 1000-block range already represents a generous upper bound. The constant is defined as a `static constexpr` in the handler to match the existing pattern for `kMaxDataSize`. The spec explicitly chose `-32602` (invalid params) for this error.

**Alternatives considered**:
- Configurable via `config.json` — Over-engineering for this spec; can be added in a future configuration iteration.
- No limit — Risks OOM on large chains with data-heavy blocks.

## R4: getBlockRange End-Index Clamping Behavior

**Decision**: Clamp `endIndex` to `chainLength - 1` silently when it exceeds chain length. Return error only when `startIndex >= chainLength`.

**Rationale**: The spec (FR-007) explicitly requires clamping without error for end index overflow. This is the more client-friendly behavior — a block explorer requesting "blocks 990–1010" on a 1000-block chain gets blocks 990–999 without having to know the exact chain length first.

**Alternatives considered**:
- Return error for any out-of-range index — Rejected by spec; creates unnecessary client-side complexity.

## R5: Response Format Consistency

**Decision**: Use `resultJsonMessage()` for `getNodeStatus` (returns JSON object), `resultMessage()` with `json::array().dump()` for `getBlockRange` (returns JSON array), and `resultMessage()` with integer-as-string for `getChainLength`/`getChunkCount`.

**Rationale**: Matches existing patterns exactly: `listPeers` uses `resultJsonMessage()` for objects, `getBlocksByKeys` uses `resultMessage()` with array dump, and simple string/number results use `resultMessage()`.

**Alternatives considered**: None — consistency with existing codebase is the only reasonable choice.

## R6: Test Strategy

**Decision**: Single new test file `tests/rpc_expansion_tests.cpp` with Catch2, testing all 4 methods through the existing mock/direct-call patterns.

**Rationale**: Existing tests in `server_tests.cpp` test RPC infrastructure (SSL, sync gates). Method-specific logic tests are in separate files (e.g., `merkle_tests.cpp`, `block_propagation_tests.cpp`). A dedicated file for the 4 new methods keeps tests organized and independently runnable.

**Alternatives considered**:
- Add to `server_tests.cpp` — Would bloat an already infrastructure-focused test file.
- Separate file per method — Over-fragmented for 4 simple methods.
