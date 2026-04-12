# Tasks: Code Audit Remediation

**Input**: Design documents from `/specs/016-audit-remediation/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Included — constitution §III requires full test coverage with new tests for each bug fix.

**Organization**: Tasks grouped by user story (6 stories from spec.md) to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: Verify baseline before making changes

- [ ] T001 Verify clean build with `make -j8` and all existing test binaries pass

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared test infrastructure needed by all user story test tasks

**⚠️ CRITICAL**: Test tasks in all user stories depend on this helper module

- [ ] T002 Create shared test utilities module in tests/TestHelpers.hpp providing createTestDir(), cleanupTestDir(), defaultConsensusConfig(), mineTestBlock(), and buildValidChain()

**Checkpoint**: Foundational test infrastructure ready — user story work can begin

---

## Phase 3: User Story 1 — Correct Block Count and Consensus Integrity (Priority: P1) 🎯 MVP

**Goal**: Fix `getChainBlockCount()` to return the cached total and fix the premature `dirty_` flag clear so consensus, difficulty, and chain-replacement decisions use accurate data.

**Independent Test**: Build a chain across multiple chunks, free older chunks, and verify `getChainBlockCount()` returns the correct total. Verify `dirty_` stays `true` through chunk rotation until the new block is appended.

### Implementation for User Story 1

- [ ] T003 [US1] Fix getChainBlockCount() to return totalBlockCount_ instead of iterating freed chunks in src/Blockchain.cpp
- [ ] T004 [US1] Fix dirty_ flag premature clear in publish() — move clear to after chunk save in src/Blockchain.cpp

### Tests for User Story 1

- [ ] T005 [P] [US1] Add tests for block count correctness across freed chunks in tests/blockchain_tests.cpp (covers edge cases: empty chain with zero chunks, all chunks freed from memory)
- [ ] T006 [P] [US1] Add test for dirty_ flag behavior during chunk rotation in tests/lifecycle_tests.cpp (covers edge case: dirty_ checked during concurrent save+append)

**Checkpoint**: Block count is always accurate regardless of freed chunks. `dirty_` flag is consistent through publish.

---

## Phase 4: User Story 2 — Thread-Safe Concurrent Operation (Priority: P1)

**Goal**: Enforce single-threaded `io_context::run()` with a runtime assertion and replace the pending pool linear scan with an O(1) data structure.

**Independent Test**: Inspect `main.cpp` for the thread-count assertion and documentation. Run block propagation tests to verify O(1) eviction behavior.

### Implementation for User Story 2

- [ ] T007 [P] [US2] Add single-threaded io_context::run() guard (std::atomic<int> + runtime abort if count != 1, not assert()) and threading model documentation comment in src/main.cpp (validates SC-002: single-threaded enforcement eliminates data races by construction)
- [ ] T008 [P] [US2] Replace pending_pool_ with pending_map_ (unordered_map) + pending_order_ (deque) in src/BlockPropagation.hpp and implement O(1) insert/evict/expire/lookup in src/BlockPropagation.cpp

### Tests for User Story 2

- [ ] T009 [US2] Add tests for pending pool O(1) eviction and insertion-order behavior in tests/block_propagation_tests.cpp

**Checkpoint**: Single-threaded model is enforced at runtime. Pending pool operations are O(1).

---

## Phase 5: User Story 3 — Efficient Difficulty Calculation (Priority: P2)

**Goal**: Cache difficulty per adjustment boundary and retain chunks during multi-access operations so difficulty calculations don't cause O(W×D) chunk load/free cycles.

**Independent Test**: Build a long chain, call difficulty calculation, and verify the same chunk is not loaded from disk more than once per calculation.

### Implementation for User Story 3

- [ ] T010 [US3] Add difficultyCache_ (unordered_map<size_t, uint32_t>), retainedChunks_ (set<size_t>), and ChunkRetainGuard RAII class to src/Blockchain.hpp
- [ ] T011 [US3] Implement chunk retention — make freeChunk() a no-op for retained chunks, add retainChunk()/releaseChunks() in src/Blockchain.cpp
- [ ] T012 [US3] Implement difficulty caching in getDifficultyForHeight() — check/populate difficultyCache_ per adjustment boundary, wrap in ChunkRetainGuard in src/Blockchain.cpp
- [ ] T013 [US3] Apply ChunkRetainGuard in recoverChain() validation loop and invalidate difficultyCache_ on replaceChain()/recoverChain() in src/Blockchain.cpp

### Tests for User Story 3

- [ ] T014 [P] [US3] Add tests for difficulty cache hit/miss and invalidation on replaceChain in tests/consensus_tests.cpp (use MockChunk to count loadChunk() calls and verify same chunk is not loaded more than once per calculation)
- [ ] T015 [P] [US3] Add tests for chunk retention during multi-access operations in tests/chunk_persistence_tests.cpp (use MockChunk to verify freeChunk() is a no-op while ChunkRetainGuard is active)

**Checkpoint**: Difficulty calculation is cached per boundary. Chunks stay loaded during multi-block scans.

---

## Phase 6: User Story 4 — Reduced Code Duplication (Priority: P2)

**Goal**: Extract shared utilities for chunk filenames, RPC responses, peer sending, stream entry lookup, and test setup so each pattern has a single source of truth.

**Independent Test**: Grep for removed patterns to confirm zero remaining inline duplications (see quickstart.md verification commands).

### Implementation for User Story 4

- [ ] T016 [P] [US4] Add chunkFilename(size_t index) returning "chunk_NNNNNN.dat" to src/utils.hpp and src/utils.cpp
- [ ] T017 [P] [US4] Add static makeJsonRpcError(id, code, message, data) and makeJsonRpcResult(id, result) helpers to src/network/RpcServer.cpp
- [ ] T018 [P] [US4] Add send_to_peers(const Block& block, const std::string& exclude_key = "") to src/PeerManager.hpp and src/PeerManager.cpp
- [ ] T019 [US4] Replace all inline chunk filename constructions (8 sites) with chunkFilename() in src/Blockchain.cpp and src/Chunk.cpp
- [ ] T020 [US4] Replace all RPC error/result boilerplate (9 methods) with makeJsonRpcError()/makeJsonRpcResult() calls in src/network/RpcServer.cpp
- [ ] T021 [US4] Replace broadcast_block() and relay_block() with send_to_peers() calls in src/PeerManager.cpp
- [ ] T022 [US4] Extract shared block-lookup lambda in getStreamEntries() with-key and without-key branches in src/Blockchain.cpp
- [ ] T023 [US4] Update test files that duplicate setup patterns to use TestHelpers.hpp (createTestDir, defaultConsensusConfig, mineTestBlock) — target files: tests/block_tests.cpp, tests/consensus_tests.cpp, tests/chunk_persistence_tests.cpp, tests/chunk_recovery_tests.cpp, tests/chunk_replace_tests.cpp, tests/lifecycle_tests.cpp, tests/lifecycle_integration_tests.cpp, tests/block_propagation_tests.cpp, tests/block_propagation_integration_tests.cpp

**Checkpoint**: All duplication clusters resolved. Each pattern maintained in a single location.

---

## Phase 7: User Story 5 — Correct Merkle Root and Block Handling (Priority: P3)

**Goal**: Add a verify-then-cache Block constructor for received blocks so the sender's merkle root is verified once on receipt and not redundantly recomputed.

**Independent Test**: Sync blocks from a peer and verify the received merkle root and hash are preserved rather than recomputed.

### Implementation for User Story 5

- [ ] T024 [US5] Add Block constructor overload accepting pre-computed merkleRoot and hash — verify merkle root against entries, throw std::invalid_argument on mismatch — in src/Block.hpp and src/Block.cpp
- [ ] T025 [US5] Update block sync/receive code to use the verify-then-cache constructor in src/BlockPropagation.cpp
- [ ] T026 [US5] Add tests for merkle root verification on received blocks (valid and invalid) in tests/block_propagation_tests.cpp

**Checkpoint**: Received blocks verify and cache the sender's merkle root. Invalid merkle roots are rejected.

---

## Phase 8: User Story 6 — Robust IPv6 Peer Identification (Priority: P3)

**Goal**: Extract a `parsePeerKey()` utility that correctly handles both IPv4 (`host:port`) and IPv6 (`[host]:port`) sender key formats.

**Independent Test**: Parse `[::1]:8333` and `192.168.1.1:8333` and verify correct host/port extraction.

### Implementation for User Story 6

- [ ] T027 [US6] Add parsePeerKey(const string& key) returning pair<string, uint16_t> with rfind/bracket-aware parsing to src/utils.hpp and src/utils.cpp
- [ ] T028 [US6] Replace sender_key.find(':') calls with parsePeerKey() in src/BlockPropagation.cpp
- [ ] T029 [US6] Add tests for IPv4, IPv6, and malformed peer key parsing in tests/block_propagation_tests.cpp

**Checkpoint**: IPv6 sender keys parsed correctly. Malformed keys throw with descriptive errors.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Final validation and documentation updates

- [ ] T030 Update docs/AUDIT.md with remediation status for all addressed issues (bugs, duplication, performance, thread safety)
- [ ] T031 Run all test binaries individually per quickstart.md to validate no regressions
- [ ] T032 [P] Verify shared utility adoption via grep checks from quickstart.md (zero inline chunk filenames, ≤2 jsonrpc boilerplate, zero sender_key.find)
- [ ] T033 Update docs/ROADMAP.md — move 016-audit-remediation to Completed table with summary and date (constitution §XIII)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — BLOCKS all user story test tasks
- **User Stories (Phases 3–8)**: All depend on Phase 2 for test helpers
  - US1 and US2 (both P1) can proceed in parallel
  - US3 and US4 (both P2) can proceed in parallel after P1 stories
  - US5 and US6 (both P3) can proceed in parallel after P2 stories
  - All stories are independently testable
- **Polish (Phase 9)**: Depends on all user stories being complete

### User Story Dependencies

- **US1 (P1)**: Phase 2 only — no cross-story dependencies
- **US2 (P1)**: Phase 2 only — no cross-story dependencies
- **US3 (P2)**: Phase 2 only — no cross-story dependencies (modifies Blockchain.cpp independently of US1 fixes)
- **US4 (P2)**: Phase 2 only — no cross-story dependencies (refactors are additive, do not conflict with bug fixes). Note: T019 modifies src/Blockchain.cpp which is also modified by US1 (T003, T004) and US3 (T011–T013) — coordinate merges if implementing in parallel.
- **US5 (P3)**: Phase 2 only — no cross-story dependencies
- **US6 (P3)**: Phase 2 only — no cross-story dependencies

### Within Each User Story

- Implementation tasks before test tasks (fix the bug, then validate)
- Same-file tasks are sequential
- Different-file tasks marked [P] can run in parallel
- Story complete when checkpoint criteria met

### Parallel Opportunities

**Phase 3 (US1)**: T005 ‖ T006 (different test files, after T003–T004)
**Phase 4 (US2)**: T007 ‖ T008 (different source files)
**Phase 5 (US3)**: T014 ‖ T015 (different test files, after T010–T013)
**Phase 6 (US4)**: T016 ‖ T017 ‖ T018 (different source files); then T019, T020, T021 independently after their respective helpers
**Phase 7 (US5)**: T024→T025→T026 (sequential chain)
**Phase 8 (US6)**: T027→T028→T029 (sequential chain)
**Cross-story**: US1 ‖ US2 (both P1), US3 ‖ US4 (both P2), US5 ‖ US6 (both P3)

---

## Parallel Example: User Story 4

```bash
# Launch all utility helpers in parallel (different files):
Task T016: "Add chunkFilename() to src/utils.hpp and src/utils.cpp"
Task T017: "Add makeJsonRpcError/makeJsonRpcResult to src/network/RpcServer.cpp"
Task T018: "Add send_to_peers() to src/PeerManager.hpp and src/PeerManager.cpp"

# Then apply replacements (each depends on its helper):
Task T019: "Replace inline chunk filenames in src/Blockchain.cpp and src/Chunk.cpp"  (after T016)
Task T020: "Replace RPC boilerplate in src/network/RpcServer.cpp"                    (after T017)
Task T021: "Replace broadcast/relay with send_to_peers in src/PeerManager.cpp"       (after T018)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (verify build)
2. Complete Phase 2: Foundational (TestHelpers.hpp)
3. Complete Phase 3: User Story 1 (block count + dirty flag fixes)
4. **STOP and VALIDATE**: Run `./tests/blockchain_tests` and `./tests/lifecycle_tests`
5. Block count bug and dirty flag bug resolved — highest-severity issues fixed

### Incremental Delivery

1. Setup + Foundational → Test infrastructure ready
2. US1 (block count + dirty flag) → **MVP — highest-severity bugs fixed**
3. US2 (thread safety + pending pool) → Runtime safety enforced
4. US3 (difficulty cache + chunk retention) → Performance issues resolved
5. US4 (deduplication) → Maintenance burden reduced
6. US5 (merkle root) → Sync efficiency improved
7. US6 (IPv6) → Network compatibility complete
8. Polish → AUDIT.md updated, full regression pass

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup + Foundational together
2. Once Foundational is done:
   - Developer A: US1 (block count) + US3 (difficulty cache) — both touch Blockchain.cpp but different functions
   - Developer B: US2 (thread safety) + US5 (merkle root) — main.cpp + BlockPropagation
   - Developer C: US4 (deduplication) — cross-cutting refactors
   - Developer D: US6 (IPv6) — utils + BlockPropagation
3. Note: US4 T019 and US3 both touch Blockchain.cpp — coordinate merges

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable
- Commit after each task or logical group
- All builds must use `make -j8`
- Run each test binary individually (not `make check`)
- After all tasks complete, update docs/AUDIT.md per user request
