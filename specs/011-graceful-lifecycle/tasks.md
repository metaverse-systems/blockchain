# Tasks: Graceful Multi-Chunk Shutdown & Startup

**Input**: Design documents from `/specs/011-graceful-lifecycle/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/config-persistence.md

**Tests**: Included — constitution (Principle III) requires full test coverage for every new feature.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Build system and test infrastructure for the feature

- [X] T001 Add `lifecycle_tests` and `lifecycle_integration_tests` test targets to `tests/Makefile.am`
- [X] T002 [P] Add `lifecycle_tests` and `lifecycle_integration_tests` to `.gitignore`
- [X] T003 [P] Create test scaffold for `tests/lifecycle_tests.cpp` with Catch2 includes and empty test cases

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Per-chunk dirty flag infrastructure and shutdown freeze flag — both are required by ALL user stories

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T004 Add `dirty_` bool field, `isDirty()`, `markDirty()`, `clearDirty()` methods to `IChunk` in `src/IChunk.hpp`; intercept `push_back()`, `emplace_back()`, and `resize()` to call `markDirty()` automatically
- [X] T005 Update `Chunk::save()` in `src/Chunk.cpp` to call `clearDirty()` after successful write
- [X] T006 Update `Chunk::load()` in `src/Chunk.cpp` to call `clearDirty()` after successful load
- [X] T007 Add `shutting_down_` bool field, `isShuttingDown()`, and `setShuttingDown()` methods to `IBlockchain` in `src/IBlockchain.hpp`
- [X] T008 Add `fast_startup` bool field (default: `false`) to `PersistenceConfig` struct in `src/NodeConfig.hpp`
- [X] T009 Parse `fast_startup` from `persistence` section in `NodeConfig::load()` in `src/NodeConfig.cpp`; add to known-keys validation and default config generation
- [X] T010 Update `config.README` generation in `NodeConfig::generate_readme()` in `src/NodeConfig.cpp` to document the `fast_startup` option

**Checkpoint**: Foundation ready — dirty tracking, shutdown freeze, and fast_startup config are in place

---

## Phase 3: User Story 1 - Complete Data Preservation on Shutdown (Priority: P1) 🎯 MVP

**Goal**: On SIGINT/SIGTERM, freeze block ingestion, then save all dirty chunks and indexes to disk

**Independent Test**: Stop a running node with blocks spanning multiple chunks; restart and verify all blocks are preserved

### Tests for User Story 1

- [X] T011 [P] [US1] Write unit test in `tests/lifecycle_tests.cpp`: verify `saveAllChunks()` saves only dirty chunks (create 3 MockChunks, mark 2 dirty, assert only those 2 are saved)
- [X] T012 [P] [US1] Write unit test in `tests/lifecycle_tests.cpp`: verify `saveAllChunks()` skips empty chunks (zero blocks) even if marked dirty
- [X] T013 [P] [US1] Write unit test in `tests/lifecycle_tests.cpp`: verify `appendBlock()` throws/rejects when `shutting_down_` is true
- [X] T014 [P] [US1] Write unit test in `tests/lifecycle_tests.cpp`: verify `publish()` throws/rejects when `shutting_down_` is true

### Implementation for User Story 1

- [X] T015 [US1] Rewrite `Blockchain::saveAllChunks()` in `src/Blockchain.cpp` to iterate all chunks in `chain` vector; save each chunk where `isDirty() == true && size() > 0`; log errors per-chunk and continue on failure; save keys/streams/streamIndex after chunks
- [X] T016 [US1] Add `shutting_down_` guard to `Blockchain::appendBlock()` in `src/Blockchain.cpp` — throw `std::runtime_error` if `isShuttingDown()` returns true
- [X] T017 [US1] Add `shutting_down_` guard to `Blockchain::publish()` in `src/Blockchain.cpp` — throw `std::runtime_error` if `isShuttingDown()` returns true
- [X] T018 [US1] Update signal handler in `src/main.cpp` to call `bc.setShuttingDown()` as the first action before `stopPeriodicSave()`, `save_peers()`, and `saveAllChunks()`

**Checkpoint**: Shutdown now preserves all dirty chunks. US1 is independently testable via `make check`

---

## Phase 4: User Story 2 - Full Chain Restore on Startup (Priority: P1)

**Goal**: On startup, discover all chunk files, validate them, load the full valid chain with placeholders for historical chunks

**Independent Test**: Start a node against a data directory with 5 chunk files; verify correct total block count and block accessibility from any chunk

### Tests for User Story 2

- [X] T019 [P] [US2] Write unit test in `tests/lifecycle_tests.cpp`: verify `recoverChain()` loads all valid chunks and reports correct `totalBlockCount_` and `chunkCount_`
- [X] T020 [P] [US2] Write unit test in `tests/lifecycle_tests.cpp`: verify `recoverChain()` stops at corrupted chunk and operates with valid prefix
- [X] T021 [P] [US2] Write unit test in `tests/lifecycle_tests.cpp`: verify `recoverChain()` detects cross-chunk linkage break and loads only the valid prefix
- [X] T022 [P] [US2] Write unit test in `tests/lifecycle_tests.cpp`: verify `recoverChain()` with no chunk files on disk creates fresh genesis chain

### Implementation for User Story 2

- [X] T023 [US2] Refactor `recoverChain()` in `src/Blockchain.cpp` to properly handle multi-chunk startup: create placeholders for all valid chunks, load only the active (last) chunk, free historical chunks after validation. Index rebuild logic is handled by T024c
- [X] T024 [US2] Ensure `recoverChain()` logs a success message with total block count and chunk count on successful full-chain recovery in `src/Blockchain.cpp`
- [X] T024b [P] [US2] Write unit test in `tests/lifecycle_tests.cpp`: verify `recoverChain()` rebuilds key/stream/stream-key indexes from chunk data when index files (`keys.dat`, `streams.dat`, `stream_index.dat`) are missing but chunk files are valid
- [X] T024c [US2] Ensure `recoverChain()` in `src/Blockchain.cpp` explicitly detects missing index files and triggers a full index rebuild by iterating all blocks across all valid chunks

**Checkpoint**: Startup now discovers and loads the full chain. US1 + US2 form the complete shutdown/restart cycle

---

## Phase 5: User Story 3 - Per-Chunk Dirty Tracking (Priority: P2)

**Goal**: Track which chunks have been modified so that saves only write changed chunks

**Independent Test**: Modify blocks in one chunk only; trigger a save and verify only that chunk is written

### Tests for User Story 3

- [X] T025 [P] [US3] Write unit test in `tests/lifecycle_tests.cpp`: verify `IChunk::push_back()` sets `dirty_` to true
- [X] T026 [P] [US3] Write unit test in `tests/lifecycle_tests.cpp`: verify `Chunk::save()` clears `dirty_` flag
- [X] T027 [P] [US3] Write unit test in `tests/lifecycle_tests.cpp`: verify `Chunk::load()` clears `dirty_` flag
- [X] T028 [P] [US3] Write unit test in `tests/lifecycle_tests.cpp`: verify newly constructed chunk has `dirty_ == false`
- [X] T029 [P] [US3] Write unit test in `tests/lifecycle_tests.cpp`: verify periodic save (`startPeriodicSave`) only writes dirty chunks via the updated `saveAllChunks()`

### Implementation for User Story 3

- [X] T030 [US3] Verify `Blockchain::publish()` in `src/Blockchain.cpp` correctly marks the target chunk dirty via `push_back()` auto-marking (no manual marking needed since T004 hooks mutations)
- [X] T031 [US3] Verify `Blockchain::appendBlock()` in `src/Blockchain.cpp` correctly marks the target chunk dirty via `push_back()` auto-marking
- [X] T032 [US3] Ensure `Blockchain::replaceChain()` in `src/Blockchain.cpp` marks all rebuilt chunks dirty (the existing `push_back` loop already triggers auto-marking from T004; verify and log)

**Checkpoint**: Dirty tracking is complete. Saves are now I/O-efficient

---

## Phase 6: User Story 4 - Startup Integrity Verification (Priority: P2)

**Goal**: Validate chunk file integrity and cross-chunk linkage on startup; support fast_startup to skip validation

**Independent Test**: Corrupt a byte in a chunk file, start the node, verify it detects and reports corruption

### Tests for User Story 4

- [X] T033 [P] [US4] Write unit test in `tests/lifecycle_tests.cpp`: verify `recoverChain()` with `fast_startup=true` skips validation and loads all discovered chunks
- [X] T034 [P] [US4] Write unit test in `tests/lifecycle_tests.cpp`: verify `recoverChain()` with `fast_startup=false` (default) validates each chunk and stops on corruption

> Note: T020 and T034 test overlapping scenarios (corruption detection). Implementers may share a helper function for creating corrupted chunk fixtures.

- [X] T035 [P] [US4] Write unit test in `tests/lifecycle_tests.cpp`: verify zero-byte chunk file is treated as corrupted and excluded from valid prefix

### Implementation for User Story 4

- [X] T036 [US4] Add `fast_startup` parameter to `recoverChain()` signature in `src/Blockchain.hpp` and `src/Blockchain.cpp`; when true, skip the `validateChunk()` loop and cross-chunk linkage checks
- [X] T037 [US4] Pass `node_config.persistence.fast_startup` to `bc.recoverChain()` call in `src/main.cpp`
- [X] T038 [US4] Add zero-byte file detection in `validateChunk()` in `src/Blockchain.cpp` — return false if chunk file exists but has zero size

**Checkpoint**: All 4 user stories are complete. Full lifecycle is now robust

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Integration tests, build verification, and documentation updates

- [X] T039 [P] Create integration test file `tests/lifecycle_integration_tests.cpp` with Catch2; test multi-chunk shutdown/restart cycle: create chain with 3+ chunks, save via `saveAllChunks()`, clear in-memory state, run `recoverChain()`, verify all blocks match
- [X] T039b [P] Write integration test in `tests/lifecycle_integration_tests.cpp`: invoke `BlockPropagation::appendReceivedBlock()` after `bc.setShuttingDown()` and verify the block is rejected (covers US1 acceptance scenario 5 end-to-end through the propagation layer)
- [X] T040 [P] Verify `lifecycle_integration_tests` test target exists in `tests/Makefile.am` (added in T001); fix if missing
- [X] T041 Run `make check` to verify all existing and new tests pass
- [X] T042 Update `docs/ROADMAP.md`: move spec 011 from "Suggested Specs" to "Completed" table with one-line summary; update "Last updated" date
- [X] T043 Run quickstart.md validation steps against a local node to verify end-to-end behavior

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on T001 for test targets — BLOCKS all user stories
- **US1 Shutdown (Phase 3)**: Depends on Phase 2 completion (T004–T010)
- **US2 Startup (Phase 4)**: Depends on Phase 2 completion; independent of US1
- **US3 Dirty Tracking (Phase 5)**: Depends on Phase 2 (T004 dirty flag); can run in parallel with US1/US2
- **US4 Integrity Verification (Phase 6)**: Depends on Phase 2 (T008–T009 fast_startup config)
- **Polish (Phase 7)**: Depends on all user stories being complete

### User Story Dependencies

- **US1 (P1)**: Foundational only — no other story dependencies
- **US2 (P1)**: Foundational only — no other story dependencies
- **US3 (P2)**: Foundational only — tests validate T004 behavior
- **US4 (P2)**: Foundational only — uses fast_startup from T008–T009

### Within Each User Story

- Tests written FIRST, verified to FAIL before implementation
- Implementation tasks sequenced: core logic → guards → integration points
- Story complete before checkpoint

### Parallel Opportunities

**Phase 2** (after T001):
```
T004 (IChunk dirty) | T007 (shutting_down_ flag) | T008 (fast_startup config)
T005, T006 (after T004) | T009, T010 (after T008)
```

**Phase 3–6** (after Phase 2 complete):
```
US1 tests (T011–T014) can all run in parallel
US2 tests (T019–T022) can all run in parallel
US3 tests (T025–T029) can all run in parallel
US4 tests (T033–T035) can all run in parallel
```

**Across stories** (after Phase 2):
```
US1 (Phase 3) | US3 (Phase 5)  — fully independent, different code paths
US2 (Phase 4) | US4 (Phase 6)  — US4 extends US2's recoverChain
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — blocks all stories)
3. Complete Phase 3: US1 — Shutdown saves all dirty chunks
4. **STOP and VALIDATE**: Test shutdown/save independently
5. This alone fixes the primary data-loss bug

### Incremental Delivery

1. Setup + Foundational → Foundation ready
2. US1 (Shutdown) → Primary bug fix (MVP!)
3. US2 (Startup) → Complete shutdown/restart cycle
4. US3 (Dirty Tracking) → I/O optimization
5. US4 (Integrity + fast_startup) → Production hardening
6. Polish → Integration tests, docs, `make check`

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story
- Constitution requires tests (Principle III); all user stories include test tasks
- Commit after each task or logical group
- Stop at any checkpoint to validate independently
