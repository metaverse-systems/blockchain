# Tasks: Audit Bug & Security Fixes

**Input**: Design documents from `/specs/018-audit-bug-security-fixes/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Included — constitution Principle III requires full test coverage for every feature.

**Organization**: Tasks grouped by user story for independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: No project initialization needed — all changes modify existing files. This phase validates the starting state.

- [ ] T001 Verify project builds cleanly with `make -j8` and all existing tests pass

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Harden the shared `parsePeerKey()` utility that both US3 (seed node parsing) and US5 (peer key validation) depend on.

**⚠️ CRITICAL**: US3 and US5 both depend on a hardened `parsePeerKey()`. This must be complete first.

- [ ] T002 Add port range validation [1, 65535] to `parsePeerKey()` in `src/utils.cpp`: wrap `std::stoi()` in try/catch, validate parsed integer is in [1, 65535], throw descriptive `std::invalid_argument` for out-of-range or non-numeric port strings
- [ ] T003 Create `tests/utils_tests.cpp` and add it to `blockchain_tests_SOURCES` in `tests/Makefile.am`. Add unit tests for `parsePeerKey()` port validation: test port 0 (rejected), port 70000 (rejected), port -1 (rejected), port "abc" (rejected), `[::1]:8333` (accepted), `192.168.1.1:65535` (accepted), `host:1` (accepted)

**Checkpoint**: `parsePeerKey()` is hardened — US3 and US5 can proceed.

---

## Phase 3: User Story 1 — Chain Sync Completes Successfully (Priority: P1) 🎯 MVP

**Goal**: Fix `handle_sync_response()` to actually append validated blocks to the local chain instead of silently discarding them.

**Independent Test**: Construct a `SyncResponse` with blocks above local height, call handler, verify chain height increases.

### Tests for User Story 1

- [ ] T004 [US1] Add sync handler test in `tests/sync_tests.cpp`: "handle_sync_response appends new blocks" — construct a blockchain with N blocks, build a SyncResponse with blocks N through N+K, invoke handler logic, assert `getChainBlockCount()` increases to N+K
- [ ] T005 [US1] Add sync handler test in `tests/sync_tests.cpp`: "handle_sync_response skips already-known blocks" — send blocks overlapping with local chain, verify no duplicates and existing blocks unchanged
- [ ] T006 [US1] Add sync handler test in `tests/sync_tests.cpp`: "handle_sync_response aborts on overlap hash mismatch" — send batch where an overlapping block has a different hash, verify chain height unchanged and error logged
- [ ] T007 [US1] Add sync handler test in `tests/sync_tests.cpp`: "handle_sync_response treats empty batch as end-of-sync" — send SyncResponse with empty blocks vector, verify sync stops and warning logged

### Implementation for User Story 1

- [ ] T008 [US1] Fix the block-append loop in `handle_sync_response()` in `src/network/PeerClient.cpp`: after the `if (block.index < local_height) { continue; }` guard, add overlap hash-mismatch check for blocks below local_height, then call `bc.appendBlock(block)` for each new block at or above local_height, followed by `bc.saveChunk(block.index / bc.chunkSize)` and `bc.saveKeys()`
- [ ] T009 [US1] Handle empty sync response in `handle_sync_response()` in `src/network/PeerClient.cpp`: before the existing empty-response check, add a guard that if `response.blocks.empty()` and `response.total_chain_height > local_height`, log a warning and stop requesting further batches (set `sync_status.isSyncing.store(false)`)

**Checkpoint**: Chain sync now appends received blocks. Two nodes can synchronize.

---

## Phase 4: User Story 2 — RPC Block Query Returns Error for Invalid Index (Priority: P1)

**Goal**: Add bounds check to `getBlockByIndex` RPC handler so out-of-range indices return a JSON-RPC error instead of crashing.

**Independent Test**: Send a `getBlockByIndex` RPC request with index beyond chain length, verify error -32001 returned.

### Tests for User Story 2

- [ ] T010 [P] [US2] Add RPC integration test in `tests/rpc_integration_tests.cpp`: "getBlockByIndex with out-of-range index returns error" — send request with index >= chain length, assert JSON-RPC error code -32001 and message "Block not found"
- [ ] T011 [P] [US2] Add RPC integration test in `tests/rpc_integration_tests.cpp`: "getBlockByIndex with valid index returns block" — send request with valid index, assert block data returned successfully

### Implementation for User Story 2

- [ ] T012 [US2] Add bounds check to `getBlockByIndex` handler in `src/network/RpcServer.cpp`: before calling `bc.getBlockByIndex(index)`, check `index >= bc.getChainLength()` and return `errorMessage(object["id"], -32001, "Block not found")` if out of range, following the same pattern as the `getBlockRange` handler

**Checkpoint**: RPC clients can no longer crash the node with out-of-range block queries.

---

## Phase 5: User Story 3 — Node Starts with Malformed Seed Node Arguments (Priority: P1)

**Goal**: Seed node port parsing handles invalid input gracefully instead of crashing with an uncaught exception.

**Independent Test**: Start binary with `--seed-node host:abc`, verify error message and non-zero exit.

### Tests for User Story 3

- [ ] T013 [US3] Add CLI integration test in `tests/cli_tests.cpp`: "seed node with non-numeric port exits with error" — spawn the node binary with `--seed-node host:abc`, assert non-zero exit code and stderr contains a descriptive error message. Also test `--seed-node 192.168.1.1:99999` (out-of-range port) and `--seed-node just-a-hostname` (missing colon)

### Implementation for User Story 3

- [ ] T014 [US3] Refactor seed node parsing in `src/main.cpp` to use `parsePeerKey()` from `src/utils.cpp`: replace the inline `rfind(':')`/`std::stoi()` logic with a try/catch around `parsePeerKey(seed)`, catching `std::invalid_argument`. On catch, print error to stderr (e.g., "Invalid seed node 'host:abc': [exception message]") and `return 1`. Also handle the missing-colon case (when `parsePeerKey()` throws for malformed key)

**Checkpoint**: Operators get clear error messages for malformed `--seed-node` arguments.

---

## Phase 6: User Story 4 — Chain Recovery Performs Efficiently (Priority: P2)

**Goal**: Eliminate triple chunk loading during `recoverChain()` by caching validated chunks.

**Independent Test**: Verify each chunk is deserialized from disk at most once during recovery.

### Tests for User Story 4

- [ ] T015 [US4] Add recovery test in `tests/chunk_recovery_tests.cpp`: "recoverChain loads each chunk only once" — create multiple chunk files on disk, run recoverChain, verify totalBlockCount equals sum of all chunk block counts (confirming blocks were counted from the validated chunk, not a separate load)

### Implementation for User Story 4

- [ ] T016 [US4] Change `validateChunk()` signature in `src/ChainPersistence.hpp` from `bool validateChunk(size_t chunkIndex, const ConsensusConfig& config)` to `std::optional<ChunkHandler> validateChunk(size_t chunkIndex, const ConsensusConfig& config)`
- [ ] T017 [US4] Update `validateChunk()` implementation in `src/ChainPersistence.cpp` to return the loaded `ChunkHandler` on success (`std::optional<ChunkHandler>(std::move(chunk))`) and `std::nullopt` on failure, instead of `true`/`false`
- [ ] T018 [US4] Refactor `recoverChain()` non-fast-startup loop in `src/ChainPersistence.cpp`: call `validateChunk(i, config)` and capture the returned chunk; use it directly for cross-chunk linkage check (compare `currChunk.blocks[0].prevHash` against previous chunk's last block hash); use it for block counting (`totalBlocks += currChunk.blocks.size()`); keep a `prevChunk` variable across iterations to avoid reloading chunk N-1

**Checkpoint**: Chain recovery reads each chunk file exactly once.

---

## Phase 7: User Story 5 — Peer Key Parsing Validates Port Range (Priority: P2)

**Goal**: All peer key port values outside [1, 65535] are rejected with descriptive errors.

**Independent Test**: Already covered by T003 (Foundational phase).

**Note**: The implementation (T002) and tests (T003) are in Phase 2 because this is a shared dependency. No additional tasks needed.

**Checkpoint**: Peer key parsing is complete via Phase 2 tasks.

---

## Phase 8: User Story 6 — getBlockByIndex Internal Resize Uses Correct IDs (Priority: P3)

**Goal**: Fix chunk vector resize in `getBlockByIndex` to assign correct chunk IDs to each new entry.

**Independent Test**: Request a block in a high chunk index that triggers multi-slot resize, verify each new entry's chunk ID.

### Tests for User Story 6

- [ ] T019 [US6] Add unit test in `tests/block_tests.cpp`: "getBlockByIndex resize assigns correct chunk IDs" — create a blockchain with 2 chunks, request a block that would be in chunk 5, verify the intermediate chunk entries at positions 2, 3, 4 each have chunk IDs matching their position index

### Implementation for User Story 6

- [ ] T020 [US6] Fix `getBlockByIndex()` resize in `src/Blockchain.cpp`: replace `this->chain.resize(chunkIndex + 1, ChunkHandler(chunkIndex + 1, this->blockchainPath))` with a `while (this->chain.size() <= chunkIndex)` loop using `this->chain.emplace_back(ChunkHandler(this->chain.size(), this->blockchainPath))`, matching the pattern in `appendBlock()`

**Checkpoint**: Chunk vector entries always have correct IDs.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Final validation and roadmap update.

- [ ] T021 Build the full project with `make -j8` and verify zero compiler warnings related to changed files
- [ ] T022 Run all test binaries individually and verify all pass: `./tests/blockchain_tests`, `./tests/rpc_integration_tests`, `./tests/chunk_recovery_tests`, `./tests/cli_tests`
- [ ] T023 Run quickstart.md manual verification steps for §3.1 (seed node parsing) to confirm error messages are user-friendly
- [ ] T024 Update `docs/ROADMAP.md` to move 018-audit-bug-security-fixes to Completed with a one-line summary

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — verify starting state
- **Phase 2 (Foundational)**: Depends on Phase 1 — BLOCKS Phase 5 and Phase 7; also used by Phase 5 (US3)
- **Phase 3 (US1)**: Depends on Phase 1 only — no dependency on Phase 2
- **Phase 4 (US2)**: Depends on Phase 1 only — no dependency on Phase 2
- **Phase 5 (US3)**: Depends on Phase 2 (hardened `parsePeerKey()`)
- **Phase 6 (US4)**: Depends on Phase 1 only — no dependency on other stories
- **Phase 7 (US5)**: Completed by Phase 2 tasks
- **Phase 8 (US6)**: Depends on Phase 1 only — no dependency on other stories
- **Phase 9 (Polish)**: Depends on all preceding phases

### User Story Dependencies

- **US1 (P1)**: Independent — can start after Phase 1
- **US2 (P1)**: Independent — can start after Phase 1
- **US3 (P1)**: Depends on Phase 2 (hardened `parsePeerKey()`)
- **US4 (P2)**: Independent — can start after Phase 1
- **US5 (P2)**: Completed in Phase 2
- **US6 (P3)**: Independent — can start after Phase 1

### Within Each User Story

- Tests written first (Catch2)
- Implementation follows
- Checkpoint verification after implementation

### Parallel Opportunities

After Phase 1 completes, the following can run in parallel:

- **Stream A**: Phase 2 (T002–T003) → Phase 5 (T013–T014)
- **Stream B**: Phase 3 (T004–T009) — US1 sync fix
- **Stream C**: Phase 4 (T010–T012) — US2 RPC bounds check
- **Stream D**: Phase 6 (T015–T018) — US4 recovery optimization
- **Stream E**: Phase 8 (T019–T020) — US6 resize fix

---

## Implementation Strategy

**MVP**: User Story 1 (sync appends blocks) — fixes the most critical functional break.

**Incremental delivery**:
1. Phase 1 → Phase 2 → all remaining phases in parallel
2. P1 stories (US1, US2, US3) deliver the critical fixes
3. P2 stories (US4, US5) deliver correctness and performance improvements
4. P3 story (US6) delivers a defensive correctness fix
