# Tasks: Address Test Quality

**Input**: Design documents from `/specs/020-address-test-quality/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the interface split and shared test infrastructure that all user stories depend on.

- [X] T001 Create `IChainReader` interface header in `src/IChainReader.hpp` with 10 read-only virtual methods per contracts/interfaces.md
- [X] T002 Create `IChainWriter` interface header in `src/IChainWriter.hpp` with 7 mutation virtual methods per contracts/interfaces.md
- [X] T003 Modify `src/IBlockchain.hpp` to inherit from `IChainReader` and `IChainWriter`, removing duplicated method declarations now inherited from sub-interfaces
- [X] T004 Update `src/Blockchain.hpp` and `src/Blockchain.cpp` to compile against the new inheritance hierarchy (IBlockchain → IChainReader + IChainWriter)
- [X] T005 Update all consumers that include `IBlockchain.hpp` to also include the new sub-interface headers where needed (ensure `make -j8` builds clean)
- [X] T006 Add `friend class RpcHandlerTests;` declaration to `src/network/RpcServer.hpp` to enable direct handler testing
- [X] T007 Change `saveAllChunks()` in `src/ChainPersistence.hpp` to return `size_t` (failure count) and only set `dirty = false` when all operations succeed
- [X] T008 Update `src/Blockchain.hpp` and `src/Blockchain.cpp` for the new `saveAllChunks()` return type (callers that discard the return value need no changes)
- [X] T009 Create `MockChainReader` class in `tests/TestHelpers.hpp` implementing `IChainReader` with configurable return values for all 10 methods
- [X] T010 [P] Create `MockChainWriter` class in `tests/TestHelpers.hpp` implementing `IChainWriter` with call-recording for all 7 methods
- [X] T011 Build and run existing tests to confirm no regressions from the interface split and `saveAllChunks()` contract change (`make -j8` then run each test binary individually)

**Checkpoint**: Interface split complete, mock infrastructure ready, all existing tests pass.

---

## Phase 2: Foundational (Assertion Audit Inventory)

**Purpose**: Systematic audit of all 26 test files to build a complete inventory of trivial/vacuous assertions. This inventory drives US1 and US2 implementation.

- [X] T012 Audit all 26 test files for `REQUIRE(true)`, `SUCCEED(...)`, and `REQUIRE_NOTHROW(...)` used as sole assertions; produce a checklist in `specs/020-address-test-quality/assertion-audit.md` listing every flagged test case with file, line, test name, and proposed replacement assertion

**Checkpoint**: Complete inventory of every test needing assertion improvement across all 26 files.

---

## Phase 3: User Story 1 — Replace Trivial Assertions (Priority: P1) 🎯 MVP

**Goal**: Every test case across all 26 files has at least one assertion checking an observable side-effect or return value. No `REQUIRE(true)` or `SUCCEED(...)` as sole assertion.

**Independent Test**: `grep -rn 'REQUIRE(true)\|SUCCEED(' tests/` returns zero matches for sole-assertion patterns. Every updated test fails when its target behavior is broken.

### Implementation for User Story 1

- [X] T013 [P] [US1] Replace trivial assertions in `tests/server_tests.cpp` (3 instances: "Server Construction" L51, "P2P mutual TLS context rejects missing peer cert" L130, and L229) with assertions checking observable server/SSL properties
- [X] T014 [P] [US1+US2] Replace `SUCCEED(...)` in `tests/block_propagation_tests.cpp` (2 instances: "FIFO eviction capacity" L49, "Pending pool capacity eviction" L327) with assertions verifying dedup cache size, pool size equals capacity (64), and oldest-entry eviction via chain height after resolution; verify each assertion is mutation-sensitive (absorbs former T021)
- [X] T015 [P] [US1+US2] Replace `SUCCEED(...)` in `tests/consensus_tests.cpp` ("Difficulty cache invalidated" L560) with assertion that cached difficulty before `replaceChain` differs from recomputed difficulty after; verify mutation-sensitivity (absorbs former T022)
- [X] T016 [P] [US1] Replace `SUCCEED(...)` in `tests/chunk_persistence_tests.cpp` ("ChunkRetainGuard RAII cleanup" L332) with assertion verifying the guard actually freed the chunk
- [X] T017 [P] [US1] Replace `REQUIRE_NOTHROW`-only assertions in `tests/lifecycle_tests.cpp` (L50, L61, L305) with assertions checking that only dirty chunks were saved and chunk state is correct after save
- [X] T018 [US1] Audit remaining 21 test files from the inventory (T012) and replace any additional trivial/vacuous sole-assertions found; commit per-file or per-logical-group
- [X] T019 [US1] Verify `grep -rn 'REQUIRE(true)' tests/` returns zero matches and `grep -rn 'SUCCEED(' tests/` returns zero sole-assertion matches; fix any remaining instances

**Checkpoint**: SC-001 met — zero trivial sole-assertions remain across all 26 test files.

---

## Phase 4: User Story 2 — Make Vacuously-Passing Tests Verify Real Behavior (Priority: P1)

**Goal**: Tests whose pass/fail outcome was independent of the behavior under test now fail when that behavior is broken.

**Independent Test**: For each updated test, temporarily break the target behavior → test fails.

### Implementation for User Story 2

- [X] T020 [P] [US2] Fix "Rate limiter allows up to limit then rejects" test in `tests/block_propagation_tests.cpp` to assert that blocks 11+ are actually rejected (chain height unchanged after rate-limited blocks)
- [X] T023 [P] [US2] Fix "Chain reorg deeper than maxReorgDepth is rejected" test in `tests/consensus_tests.cpp` to assert chain state unchanged AND verify the candidate chain is otherwise valid and longer (demonstrating the depth check is the sole rejection reason)
- [X] T024 [US2] Review all tests updated in US1 (T013-T018, including merged T014/T015) and verify each one fails when its target behavior is deliberately broken; fix any that still pass vacuously

> **Note**: T021 and T022 were merged into T014 and T015 respectively (Phase 3) to avoid touching the same test cases twice.

**Checkpoint**: SC-002 met — every assertion-improved test is mutation-sensitive.

---

## Phase 5: User Story 3 — Rewrite RPC Expansion Tests (Priority: P1)

**Goal**: `rpc_expansion_tests.cpp` invokes actual extracted RPC handler methods via friend access with mocked `IBlockchain`, replacing all manually-constructed response assertions.

**Independent Test**: Delete an RPC handler method → at least one test in this file fails.

### Implementation for User Story 3

- [X] T025 [US3] Rewrite `tests/rpc_expansion_tests.cpp` test infrastructure: create `RpcHandlerTests` class that constructs an `RpcServer` with a test `io_context`/`ssl::context` and a full `MockBlockchain` (inheriting `IBlockchain`, since the constructor requires `IBlockchain&`), accessing private handlers via `friend class RpcHandlerTests`
- [X] T026 [P] [US3] Rewrite `getNodeStatus` tests (3 tests) in `tests/rpc_expansion_tests.cpp` to call `handle_getNodeStatus()` directly and assert response fields match mock blockchain state
- [X] T027 [P] [US3] Rewrite `getBlockRange` tests (8 tests) in `tests/rpc_expansion_tests.cpp` to call `handle_getBlockRange()` directly and assert correct block data, boundary conditions, and error codes
- [X] T028 [P] [US3] Rewrite `getChainLength` and `getChunkCount` tests (4 tests) in `tests/rpc_expansion_tests.cpp` to call `handle_getChainLength()` and `handle_getChunkCount()` directly and assert values match mock state
- [X] T029 [US3] Verify all 15 tests in `tests/rpc_expansion_tests.cpp` exercise real handler logic by temporarily breaking a handler and confirming test failure

**Checkpoint**: SC-003 met — all RPC expansion tests exercise production handler code.

---

## Phase 6: User Story 4 — Make Integration Tests Deterministic (Priority: P2)

**Goal**: Integration tests pass reliably regardless of system load; no fixed-duration sleep dependencies.

**Independent Test**: Run each integration test binary 10 times locally; all pass.

### Implementation for User Story 4

- [X] T030 [P] [US4] Replace `sleep_for(500ms)` stabilization delay in `tests/p2p_sync_integration_tests.cpp` with a `wait_for_connection_ready()` helper that polls `getNodeStatus` until connection count matches
- [X] T031 [P] [US4] Wrap `client->call()` in `tests/rpc_integration_tests.cpp` with a retry-with-backoff helper that verifies connection readiness before issuing RPC calls
- [X] T032 [P] [US4] Replace `io.run_for(100ms)` in `tests/chunk_persistence_tests.cpp` timer tests with deterministic `io.poll()` / `io.run_one()` event-loop advancement
- [X] T033 [US4] Run each integration test binary 10 times locally and confirm zero flaky failures: `for i in {1..10}; do ./tests/p2p_sync_integration_tests && ./tests/rpc_integration_tests && ./tests/lifecycle_integration_tests && echo "PASS $i"; done`

**Checkpoint**: SC-004 met — 10 consecutive local runs pass without timing-related failures.

---

## Phase 7: User Story 5 — Close Coverage Gaps (Priority: P2)

**Goal**: Add test coverage for all 6 open audit coverage gaps (1 high-severity + 5 medium-severity).

**Independent Test**: Each new test runs standalone and verifies a previously-untested behavior.

### Implementation for User Story 5

- [X] T034 [P] [US5] Add test for partial `saveAllChunks()` failure in `tests/lifecycle_tests.cpp`: use a `MockChunk` that throws on `save()` for one chunk, assert return value > 0, assert `dirty` flag remains true, assert other chunks were still saved
- [X] T035 [P] [US5] Add test for peer disconnect during block propagation in `tests/block_propagation_tests.cpp`: provide a `RelayCallback` that throws/disconnects mid-relay, assert no crash and chain state is consistent
- [X] T036 [P] [US5] Add test for rate limiter window reset in `tests/block_propagation_tests.cpp`: submit blocks up to limit, wait >1 second for window to expire, submit another block and assert it is accepted (chain height increases). **Depends on T020** (same file, rate-limiter context)
- [X] T037 [P] [US5] Add test for pending pool TTL expiry in `tests/block_propagation_tests.cpp`: insert gap blocks, use injected clock or reduced TTL interval to trigger expiry (do NOT use real 60s wait — violates FR-004/SC-006), submit connecting block and assert expired entries were not resolved (chain height reflects only non-expired blocks)
- [X] T038 [P] [US5] Add test for block relay sender exclusion in `tests/block_propagation_tests.cpp`: provide a test `RelayCallback`, submit a block with sender key "peer1", assert callback is invoked with block and sender key "peer1" (enabling caller to exclude sender)
- [X] T039 [P] [US5] Add test for `recoverChain()` with corrupted index files in `tests/chunk_recovery_tests.cpp`: create a test directory with valid chunk files but corrupted `keys.dat`/`streams.dat`, run `recoverChain()`, assert chain is rebuilt from chunks and block count is correct

**Checkpoint**: SC-005 met — all 6 open coverage gaps have corresponding test cases.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Final validation, documentation, and cleanup.

- [X] T040 Build the full project with `make -j8` and run every test binary individually to confirm all tests pass; time each binary and verify none exceeds 30 seconds (SC-006)
- [X] T041 Run `grep -rn 'REQUIRE(true)\|SUCCEED(' tests/` and confirm zero sole-assertion matches remain (SC-001 final validation)
- [X] T042 Update `docs/AUDIT.md` to mark test-quality items §7.1, §7.2, §7.3, §7.4, §7.5 as resolved with cross-references to the specific changes made, update the summary table counts, and close the remaining open recommendations (#5, #6, #12)
- [X] T043 Update `docs/ROADMAP.md` to move 020-address-test-quality from in-progress/suggested to completed with a one-line summary
- [X] T044 Run quickstart.md validation: execute the determinism verification loop and the trivial-assertion grep check from `specs/020-address-test-quality/quickstart.md`

**Checkpoint**: All success criteria (SC-001 through SC-006) verified. Documentation updated. Feature complete.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Audit Inventory)**: Can start in parallel with Phase 1
- **Phase 3 (US1)**: Depends on Phase 1 (interface split for MockChainReader) + Phase 2 (audit inventory)
- **Phase 4 (US2)**: Depends on Phase 3 (assertions must exist before verifying mutation-sensitivity)
- **Phase 5 (US3)**: Depends on Phase 1 (friend declaration + MockChainReader/MockChainWriter)
- **Phase 6 (US4)**: Depends on Phase 1 only (no assertion changes needed)
- **Phase 7 (US5)**: Depends on Phase 1 (saveAllChunks contract change + mocks)
- **Phase 8 (Polish)**: Depends on all previous phases

### User Story Independence

- **US1 (P1)** + **US2 (P1)**: Sequential (US2 validates US1 changes)
- **US3 (P1)**: Independent of US1/US2 after Phase 1; can run in parallel
- **US4 (P2)**: Independent of US1/US2/US3 after Phase 1; can run in parallel
- **US5 (P2)**: Independent of US1/US2/US3/US4 after Phase 1; can run in parallel

### Parallel Opportunities

After Phase 1 completes, **US3, US4, and US5 can all proceed in parallel** since they modify different test files and have no mutual dependencies:
- US3: `rpc_expansion_tests.cpp`
- US4: `p2p_sync_integration_tests.cpp`, `rpc_integration_tests.cpp`, `chunk_persistence_tests.cpp`
- US5: `lifecycle_tests.cpp`, `block_propagation_tests.cpp`, `chunk_recovery_tests.cpp`

---

## Parallel Example: After Phase 1

```
# These can proceed simultaneously:
# Worker A: US1 (T013-T019) → US2 (T020-T024)
# Worker B: US3 (T025-T029)
# Worker C: US4 (T030-T033) + US5 (T034-T039)
```

---

## Implementation Strategy

### MVP First (User Stories 1 + 2 Only)

1. Complete Phase 1: Setup (interface split, mocks, friend declaration)
2. Complete Phase 2: Audit inventory
3. Complete Phase 3: US1 — Replace trivial assertions
4. Complete Phase 4: US2 — Verify mutation-sensitivity
5. **STOP and VALIDATE**: SC-001 and SC-002 met
6. This alone delivers the highest-value improvement to test quality

### Incremental Delivery

1. Setup + Audit → Foundation ready
2. US1 + US2 → Assertion quality across all 26 files (MVP!)
3. US3 → RPC tests exercise real code
4. US4 → Integration tests are deterministic
5. US5 → Coverage gaps closed
6. Each story adds measurable test quality without breaking previous work

---

## Notes

- All `make` invocations use `-j8` per constitution §II
- Each test binary is run individually per constitution §III
- No new test binaries are created — new tests are added to existing files
- No new external dependencies — constitution §V satisfied
- `docs/ROADMAP.md` update required per constitution §XIII (T043)
