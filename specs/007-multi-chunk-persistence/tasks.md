# Tasks: Multi-Chunk Persistence & Recovery

**Input**: Design documents from `/specs/007-multi-chunk-persistence/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/config.md, quickstart.md

**Tests**: Required per constitution §III (Full Test Coverage).

**Organization**: Tasks grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup

**Purpose**: Configuration infrastructure shared by all user stories

- [ ] T001 Add `PersistenceConfig` struct to `src/NodeConfig.hpp` with `save_interval_seconds` field (default 300)
- [ ] T002 Parse and validate `persistence` section in `src/NodeConfig.cpp` (merge-with-defaults pattern, 0 = disabled)
- [ ] T003 Add `persistence` to default JSON in `NodeConfig::default_json()` in `src/NodeConfig.cpp`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T004 Make `Chunk::save()` atomic (write to `.tmp`, then `std::filesystem::rename`) in `src/Chunk.cpp`
- [ ] T005 [P] Add `dirty_` boolean field to `Blockchain` class in `src/Blockchain.hpp`, initialize to `false`
- [ ] T006 [P] Add `totalBlockCount_` and `chunkCount_` cached counters to `Blockchain` class in `src/Blockchain.hpp`
- [ ] T007 [P] Add `io_context_` pointer, `save_timer_`, and `save_interval_seconds_` fields to `Blockchain` class in `src/Blockchain.hpp`
- [ ] T008 [P] Add `virtual size_t getChainLength() const = 0` and `virtual size_t getChunkCount() const = 0` to `src/IBlockchain.hpp`
- [ ] T009 [P] Add `getChainLength()` and `getChunkCount()` stubs to `tests/MockBlockchain.hpp`
- [ ] T010 Add new test sources (`chunk_persistence_tests.cpp`, `chunk_recovery_tests.cpp`, `chunk_replace_tests.cpp`) to `tests/Makefile.am` check_PROGRAMS and TESTS lists; no changes to `src/Makefile.am` (test binaries only)

**Checkpoint**: Foundation ready — user story implementation can now begin

---

## Phase 3: User Story 1 — Automatic Chunk Persistence on Fill (Priority: P1) 🎯 MVP

**Goal**: Filled chunks are auto-saved to disk; all chunks saved on shutdown; freed from memory after save

**Independent Test**: Add 101+ blocks, shut down, restart, verify all blocks present

### Tests for User Story 1

- [ ] T011 [P] [US1] Write test: chunk auto-saved when it reaches capacity (100 blocks) in `tests/chunk_persistence_tests.cpp`
- [ ] T012 [P] [US1] Write test: filled chunk freed from memory after auto-save in `tests/chunk_persistence_tests.cpp`
- [ ] T013 [P] [US1] Write test: all in-memory chunks saved on shutdown call in `tests/chunk_persistence_tests.cpp`
- [ ] T014 [P] [US1] Write test: save failure (disk error) logs error and continues operation in `tests/chunk_persistence_tests.cpp`

### Implementation for User Story 1

- [ ] T015 [US1] Modify `publish()` in `src/Blockchain.cpp` to auto-save the active chunk when it reaches `chunkSize`, create a new chunk, free the filled chunk, set `dirty_ = false`, and update `chunkCount_`
- [ ] T016 [US1] Modify `appendBlock()` in `src/Blockchain.cpp` with same auto-save-on-fill logic (blocks received via P2P), set `dirty_ = true` on append, update `totalBlockCount_`
- [ ] T017 [US1] Add `saveAllChunks()` method to `Blockchain` in `src/Blockchain.cpp` that saves the active chunk and all index files; wrap `Chunk::save()` in try/catch for graceful error handling (FR-010)
- [ ] T018 [US1] Update shutdown handler in `src/main.cpp` to call `bc.saveAllChunks()` instead of `bc.saveChunk(0)`

**Checkpoint**: Chain auto-saves filled chunks, saves everything on shutdown, freed chunks reduce memory

---

## Phase 4: User Story 2 — Full Chain Recovery on Startup (Priority: P1)

**Goal**: Startup discovers all chunk files, loads only the active chunk, serves filled-chunk queries on demand

**Independent Test**: Create data directory with multiple chunk files, start daemon, verify correct block count and block queries

### Tests for User Story 2

- [ ] T019 [P] [US2] Write test: startup discovers contiguous chunk files 0..N and reports correct block count in `tests/chunk_recovery_tests.cpp`
- [ ] T020 [P] [US2] Write test: startup with no chunk files creates fresh genesis block in `tests/chunk_recovery_tests.cpp`
- [ ] T021 [P] [US2] Write test: on-demand load serves block from filled chunk and frees it after in `tests/chunk_recovery_tests.cpp`
- [ ] T022 [P] [US2] Write test: new block appended seamlessly after recovery in `tests/chunk_recovery_tests.cpp`
- [ ] T023 [P] [US2] Write test: index files loaded on startup; missing index triggers rebuild by scanning chunks in `tests/chunk_recovery_tests.cpp`

### Implementation for User Story 2

- [ ] T024 [US2] Add `discoverChunks()` method to `Blockchain` in `src/Blockchain.cpp` — enumerate `chunk_000000.dat` through `chunk_NNNNNN.dat` by probing with `std::filesystem::exists()`, stop at first gap, return count
- [ ] T025 [US2] Add `recoverChain()` method to `Blockchain` in `src/Blockchain.cpp` — call `discoverChunks()`, set `chunkCount_`/`totalBlockCount_`, load only the active (last) chunk into memory, populate `chain` vector with empty placeholders for filled chunks
- [ ] T026 [US2] Modify `getBlockByIndex()` in `src/Blockchain.cpp` to load a filled chunk from disk on demand, serve the block, and free the chunk after use (FR-003a)
- [ ] T027 [US2] Modify `getBlocksByKeys()` in `src/Blockchain.cpp` with same on-demand load/free pattern for filled chunks
- [ ] T028 [US2] Add index rebuild fallback in `src/Blockchain.cpp` — if `loadKeys()`/`loadStreams()`/`loadStreamIndex()` fails or file is missing, scan all chunk files to rebuild that index (FR-009, R7)
- [ ] T029 [US2] Modify `Blockchain` constructor in `src/Blockchain.hpp`/`src/Blockchain.cpp` to accept an optional recovery flag; when set, call `recoverChain()` instead of `generateGenesisBlock()`
- [ ] T030 [US2] Update startup sequence in `src/main.cpp` to call recovery path: discover chunks, load active chunk, load/rebuild indexes, then log block count and chunk count

**Checkpoint**: Daemon restarts with full chain state; filled chunks loaded on demand; indexes recovered

---

## Phase 5: User Story 3 — Periodic Chunk Saving (Priority: P1)

**Goal**: Active chunk saved periodically on a configurable timer; skips if not dirty

**Independent Test**: Add blocks, wait for interval, kill daemon, restart, verify blocks survived

### Tests for User Story 3

- [ ] T031 [P] [US3] Write test: periodic timer fires and saves dirty active chunk in `tests/chunk_persistence_tests.cpp`
- [ ] T032 [P] [US3] Write test: periodic timer skips save when `dirty_ == false` in `tests/chunk_persistence_tests.cpp`
- [ ] T033 [P] [US3] Write test: periodic save disabled when `save_interval_seconds == 0` in `tests/chunk_persistence_tests.cpp`
- [ ] T034 [P] [US3] Write test: also saves index files (keys, streams, stream_index) on periodic save in `tests/chunk_persistence_tests.cpp`

### Implementation for User Story 3

- [ ] T035 [US3] Add `startPeriodicSave(boost::asio::io_context &io)` method to `Blockchain` in `src/Blockchain.cpp` — create `steady_timer`, schedule recurring callback per `save_interval_seconds_`, check `dirty_`, save active chunk + indexes if dirty, reschedule
- [ ] T036 [US3] Add `stopPeriodicSave()` method to `Blockchain` in `src/Blockchain.cpp` — cancel timer
- [ ] T037 [US3] Wire periodic save into `src/main.cpp` — after recovery completes, call `bc.startPeriodicSave(io_context)` with interval from `node_config.persistence.save_interval_seconds`; cancel on shutdown
- [ ] T038 [US3] Ensure `dirty_` is set to `true` in `publish()` and `appendBlock()` when a block is added to the active chunk in `src/Blockchain.cpp`

**Checkpoint**: Active chunk periodically saved; crash loses at most one interval's worth of blocks

---

## Phase 6: User Story 4 — Corrupted Chunk Detection and Reporting (Priority: P2)

**Goal**: Startup detects corrupt/truncated chunk files, logs clear errors, loads only the valid contiguous prefix

**Independent Test**: Truncate a chunk file, restart, verify error logged and only prefix loaded

### Tests for User Story 4

- [ ] T039 [P] [US4] Write test: truncated chunk file detected and logged with chunk number and path in `tests/chunk_recovery_tests.cpp`
- [ ] T040 [P] [US4] Write test: chunk with invalid block hashes detected and logged in `tests/chunk_recovery_tests.cpp`
- [ ] T041 [P] [US4] Write test: contiguous prefix loaded up to first corrupt chunk, rest discarded in `tests/chunk_recovery_tests.cpp`
- [ ] T042 [P] [US4] Write test: gap in chunk numbering stops loading at gap in `tests/chunk_recovery_tests.cpp`
- [ ] T042a [P] [US4] Write test: chunk file with restrictive permissions (e.g., 000) detected and logged with path and error details in `tests/chunk_recovery_tests.cpp`

### Implementation for User Story 4

- [ ] T043 [US4] Add `validateChunk()` method to `Blockchain` in `src/Blockchain.cpp` — deserialize chunk in try/catch (detect truncation/corruption), then verify `block.calculateHash() == block.hash` for each block and `block[i].prevHash == block[i-1].hash` for linkage (FR-006)
- [ ] T044 [US4] Integrate `validateChunk()` into `discoverChunks()` — for each existing file, attempt load + validate; on failure log error with chunk number and file path, stop loading (contiguous prefix per FR-005/FR-011)
- [ ] T045 [US4] Handle cross-chunk linkage validation — verify first block of chunk N has `prevHash` matching last block of chunk N-1 in `src/Blockchain.cpp`

**Checkpoint**: Corrupt chunks detected, reported, and excluded; only valid prefix used

---

## Phase 7: User Story 5 — Chain Length and Chunk Count Introspection (Priority: P2)

**Goal**: `getChainLength()` and `getChunkCount()` return accurate cached values

**Independent Test**: Add known number of blocks, verify counts match

### Tests for User Story 5

- [ ] T046 [P] [US5] Write test: `getChainLength()` returns correct total across multiple chunks in `tests/chunk_persistence_tests.cpp`
- [ ] T047 [P] [US5] Write test: `getChunkCount()` returns correct count in `tests/chunk_persistence_tests.cpp`
- [ ] T048 [P] [US5] Write test: counts correct immediately after recovery in `tests/chunk_recovery_tests.cpp`
- [ ] T049 [P] [US5] Write test: counts update correctly as blocks are added in `tests/chunk_persistence_tests.cpp`

### Implementation for User Story 5

- [ ] T050 [US5] Implement `getChainLength()` in `src/Blockchain.cpp` to return `totalBlockCount_` cached value
- [ ] T051 [US5] Implement `getChunkCount()` in `src/Blockchain.cpp` to return `chunkCount_` cached value
- [ ] T052 [US5] Ensure `totalBlockCount_` is updated in `publish()`, `appendBlock()`, `replaceChain()`, and `recoverChain()` in `src/Blockchain.cpp`
- [ ] T053 [US5] Ensure `chunkCount_` is updated when new chunks are created, during recovery, and during `replaceChain()` in `src/Blockchain.cpp`

**Checkpoint**: Introspection methods return accurate values at all times

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: replaceChain archive, build integration, final validation

- [ ] T054 [P] Add `archiveChainFiles()` method to `Blockchain` in `src/Blockchain.cpp` — create `backups/<timestamp>/` directory, move all `chunk_*.dat`, `keys.dat`, `streams.dat`, `stream_index.dat` via `std::filesystem::rename()`; abort and log if directory creation fails (FR-012/FR-013)
- [ ] T055 Integrate `archiveChainFiles()` into `replaceChain()` in `src/Blockchain.cpp` — archive before clearing chain, then persist new chain's chunks and indexes
- [ ] T056 [P] Write test: `replaceChain` moves old files to timestamped backup dir in `tests/chunk_replace_tests.cpp`
- [ ] T057 [P] Write test: `replaceChain` aborts if backup dir cannot be created in `tests/chunk_replace_tests.cpp`
- [ ] T058 [P] Write test: `replaceChain` with new chain persists all new chunk files in `tests/chunk_replace_tests.cpp`
- [ ] T059 Verify `make check` passes with all new test files
- [ ] T060 Run quickstart.md validation scenarios manually
- [ ] T060a [P] Write timed validation: chunk auto-save completes within 2 seconds on a 100-block chunk (SC-002) in `tests/chunk_persistence_tests.cpp`
- [ ] T060b [P] Write timed validation: startup recovery for 100-chunk (10,000 block) chain completes within 30 seconds (SC-003) in `tests/chunk_recovery_tests.cpp`
- [ ] T060c Write network integration test: peer receives 101+ blocks via P2P, verifies chunk auto-save and recovery across restart in `tests/block_propagation_integration_tests.cpp`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 (T001-T003 for config); BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Phase 2 completion
- **US2 (Phase 4)**: Depends on Phase 2 completion; independent of US1
- **US3 (Phase 5)**: Depends on Phase 2 + US1 (`dirty_` flag usage, `saveAllChunks` pattern)
- **US4 (Phase 6)**: Depends on US2 (`discoverChunks()` method to integrate validation into)
- **US5 (Phase 7)**: Depends on Phase 2 (cached counters); independent otherwise
- **Polish (Phase 8)**: Depends on US1 + US2 (archive needs both save and recovery paths)

### User Story Dependencies

- **US1 (P1)**: Can start after Phase 2 — no dependencies on other stories
- **US2 (P1)**: Can start after Phase 2 — no dependencies on other stories
- **US3 (P1)**: Depends on US1 (`dirty_` flag and save infrastructure)
- **US4 (P2)**: Depends on US2 (`discoverChunks()` to add validation into)
- **US5 (P2)**: Can start after Phase 2 — no dependencies on other stories

### Parallel Opportunities

- T005, T006, T007, T008, T009: All foundational tasks modifying different files — fully parallel
- T011-T014: All US1 tests — parallel (same file, different test cases)
- T019-T023: All US2 tests — parallel
- T031-T034: All US3 tests — parallel
- T039-T042: All US4 tests — parallel
- T046-T049: All US5 tests — parallel
- US1 and US2 can proceed in parallel after Phase 2
- US5 can proceed in parallel with US1/US2/US3

---

## Parallel Example: After Phase 2

```
Stream A (US1):  T011-T014 → T015 → T016 → T017 → T018
Stream B (US2):  T019-T023 → T024 → T025 → T026 → T027 → T028 → T029 → T030
Stream C (US5):  T046-T049 → T050 → T051 → T052 → T053

After US1 completes:
Stream D (US3):  T031-T034 → T035 → T036 → T037 → T038

After US2 completes:
Stream E (US4):  T039-T042 → T043 → T044 → T045
```

---

## Implementation Strategy

### MVP First (User Story 1 + 2)

1. Complete Phase 1: Setup (config)
2. Complete Phase 2: Foundational (atomic save, dirty flag, counters, interface)
3. Complete Phase 3: US1 — auto-save on fill + shutdown
4. Complete Phase 4: US2 — recovery on startup + on-demand loading
5. **STOP and VALIDATE**: 200+ block chain survives restart with zero block loss
6. This is the minimum viable multi-chunk persistence

### Incremental Delivery

1. Setup + Foundational → Foundation ready
2. US1 → Chunks auto-save → Deploy/Validate
3. US2 → Full recovery + on-demand loading → Deploy/Validate (MVP!)
4. US3 → Periodic saves → Deploy/Validate
5. US4 → Corruption detection → Deploy/Validate
6. US5 → Introspection methods → Deploy/Validate
7. Polish → replaceChain archive + build + quickstart validation
