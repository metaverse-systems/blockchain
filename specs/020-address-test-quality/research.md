# Research: Address Test Quality

**Feature**: 020-address-test-quality  
**Date**: 2026-04-13

## R1: RPC Handler Testability Strategy

**Decision**: Add a `friend class RpcHandlerTests;` declaration to `RpcServer`

**Rationale**: The 20 RPC handlers are all private methods with uniform signature (`nlohmann::json handler(const nlohmann::json &request)`). The constructor dereferences the socket in its initializer list (`SessionHandler(std::move(*socket_ptr), bc)`), so constructing an `RpcServer` without a real socket is not possible without refactoring the base class. A `friend` declaration is the lowest-impact change that enables direct handler testing without modifying the class's public API or inheritance hierarchy.

**Alternatives considered**:
- Make handlers public/protected: Leaks implementation detail into the public API; violates least-privilege.
- Extract handlers into a standalone class: Larger refactor; would decouple handlers from `RpcServer` state (`bc_`, `peer_manager_`, `sync_status_`, `allowed_streams_`), requiring extensive parameter passing. Higher effort for the same test coverage.
- Test via socket (integration-style): Already covered by `rpc_integration_tests.cpp`; duplicating this approach adds SSL/async overhead and doesn't improve unit-level signal.

**Implementation detail**: The test file will define `class RpcHandlerTests` and instantiate an `RpcServer` with a real SSL socket (from a test `io_context` + `ssl::context`) plus a mock `IBlockchain`. Handlers are invoked directly via the friend access.

## R2: saveAllChunks() Partial Failure Reporting

**Decision**: Change `saveAllChunks()` to return `size_t` (count of failures) and only set `dirty = false` when all operations succeed.

**Rationale**: Current implementation catches all exceptions, logs them, and unconditionally sets `dirty = false` — silently discarding failure information. Returning a failure count is the minimal contract change: callers that ignore the return value continue to compile (implicit discard), while tests can assert on it. The `dirty` flag fix is a correctness improvement — marking clean after a failed save loses data on shutdown.

**Alternatives considered**:
- Return `std::vector<std::string>` of error messages: More information but heavier; callers would need to inspect strings. Failure count is sufficient for the test and most callers.
- Throw on first failure: Breaks the "save as much as possible" behavior which is the correct design for partial persistence.
- Return a result struct: Over-engineered for the current need.

## R3: BlockPropagation Coverage Strategy

**Decision**: Test `on_block_received()` through public API, observing behavior via `IBlockchain::getChainBlockCount()` and the `RelayCallback`.

**Rationale**: The rate limiter, pending pool, dedup cache, and relay exclusion are all exercised through the public `on_block_received()` entry point. Testing private methods (`check_rate_limit()`, `evict_expired()`, `defer_block()`) directly would couple tests to implementation. Instead:
- **Rate limiting**: Submit >10 blocks/sec from one sender; verify chain height stops growing after the limit.
- **Rate limiter reset**: Advance `steady_clock` past the 1-second window (or wait >1s); verify blocks accepted again.
- **Pending pool TTL**: Insert gap blocks, advance time past 60s TTL, submit a connecting block and verify expired entries were evicted (chain height reflects only non-expired blocks).
- **Pending pool capacity**: Submit 65 gap blocks (capacity=64) and verify the pool doesn't grow unbounded (observe via chain height after resolution).
- **Relay sender exclusion**: Provide a test `RelayCallback`; verify the callback receives the block with the sender key, allowing the caller to exclude the sender.

**Alternatives considered**:
- Add `friend` access to `BlockPropagation` internals: Would enable direct state inspection but couples tests to private implementation details.

## R4: IBlockchain Interface Split Strategy

**Decision**: Extract `IChainReader` (read-only) and `IChainWriter` (mutation) from `IBlockchain`; `IBlockchain` inherits both for backward compatibility.

**Rationale**: The current 28-method `IBlockchain` forces mock objects to stub all methods even when a consumer only uses a subset. Splitting into reader/writer sub-interfaces enables lightweight mocks. `RpcServer` query handlers only need `IChainReader`; mutation handlers (`publish`, `createStream`) need `IChainWriter`.

**Method classification**:
- `IChainReader` (10 methods): `isShuttingDown()`, `listStreams()`, `getStreamEntries()`, `getStreamEntry()`, `getChainBlockCount()`, `getChainLength()`, `getChunkCount()`, `getCurrentDifficulty()`, `getConfig()`, `isValidNewBlock()`
- `IChainWriter` (7 methods): `generateGenesisBlock()`, `publish()`, `createStream()`, `appendBlock()`, `replaceChain()`, `setShuttingDown()`, `saveKeys()`
- Kept on `IBlockchain` (11 methods): Persistence and diagnostic methods (`loadChunk`, `freeChunk`, `saveChunk`, `loadKeys`, `dumpBlocks`, `dumpKeys`, `getBlocksByKeys`, `getBlockByIndex`, `getInclusionProof`, `verifyInclusionProof`) — used primarily by internal orchestration code.

**Backward compatibility**: `IBlockchain` inherits `IChainReader, IChainWriter` so all existing code that accepts `IBlockchain&` continues to work unchanged. New code (e.g., RPC query handler tests) can accept `IChainReader&` for narrower mocking.

## R5: Integration Test Determinism Strategy

**Decision**: Keep the existing poll-with-timeout pattern but replace `std::this_thread::sleep_for` stabilization delays with condition-based readiness checks. For unit-level timer tests, use `io_context::poll()` / `run_one()`.

**Rationale**: The current `wait_for_chain_length()` and `wait_for_outbound_peers()` helpers already use bounded polling (250ms / 100ms intervals with 10s timeout). These are acceptable for integration tests that exercise real async I/O. The flakiness source is the fixed 500ms "connection stabilization" delay and `run_for(100ms)` in unit tests.

**Specific fixes**:
- `p2p_sync_integration_tests.cpp`: Remove `sleep_for(500ms)` stabilization; add a `wait_for_connection_ready()` helper that polls `getNodeStatus` until the connection count matches.
- `rpc_integration_tests.cpp`: Wrap `client->call()` in a retry loop with backoff rather than assuming immediate readiness.
- `chunk_persistence_tests.cpp`: Replace `io.run_for(100ms)` with `io.poll()` + `timer.cancel()` to drive the event loop deterministically.

## R6: Comprehensive Assertion Audit Approach

**Decision**: Systematically scan all 26 test files for `REQUIRE(true)`, `SUCCEED(...)`, and `REQUIRE_NOTHROW(...)` used as sole assertions. Replace each with assertions that check observable outcomes.

**Rationale**: `REQUIRE_NOTHROW()` is borderline — it does test something (no exception thrown), but when used alone it's equivalent to "no crash." Each instance needs evaluation: if the test name implies a specific behavioral check (e.g., "saves only dirty chunks"), the assertion must verify that behavior, not just no-throw.

**Approach**:
- Phase 1: Generate a complete inventory of all test files, test case names, and their assertion patterns.
- Phase 2: For each flagged test, determine what meaningful assertion should replace the trivial one by reading the production code it exercises.
- Phase 3: Implement the replacement assertions.
