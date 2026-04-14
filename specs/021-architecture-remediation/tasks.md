# Tasks: Architecture Remediation

**Input**: Design documents from `/specs/021-architecture-remediation/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/

**Tests**: Included per Constitution §III (Full Test Coverage). New ChainService tests and updates to existing tests for new exception types.

**Organization**: Tasks grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story (US1, US2, US3, US4)
- Exact file paths included in descriptions

---

## Phase 1: Setup

**Purpose**: Create new files and build infrastructure that all user stories depend on

- [X] T001 Create exception hierarchy in src/ChainError.hpp per contracts/ChainError.md
- [X] T004 [P] Add tests/chain_service_tests to .gitignore

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Interface changes that MUST be complete before any user story can proceed

**⚠️ CRITICAL**: All user stories depend on the widened `IChainReader` and the exception hierarchy

- [X] T005 Move `getBlockByIndex()`, `getBlocksByKeys()`, `getInclusionProof()`, `verifyInclusionProof()` declarations from src/IBlockchain.hpp to src/IChainReader.hpp per contracts/IChainReader.md
- [X] T006 Remove the four moved method declarations from src/IBlockchain.hpp (retain persistence methods, `isValidNewBlock` static, `chunkSize`)
- [X] T007 Update override declarations in src/Blockchain.hpp to match the new interface locations (methods now override IChainReader virtuals)
- [X] T008 Convert `throw std::runtime_error(...)` to `throw ValidationError(...)` / `throw PersistenceError(...)` in src/Blockchain.cpp per contracts/ChainError.md migration rules
- [X] T009 Convert log-and-continue patterns to `throw PersistenceError(...)` in src/ChainPersistence.cpp (saveAllChunks partial failure must propagate)
- [X] T010 Build with `make -j8` and verify all existing tests still pass (no regressions from interface moves and exception changes)

**Checkpoint**: Interfaces widened, exceptions standardized, all existing tests green

---

## Phase 3: User Story 1 — Read-Only Consumers See Only Query Methods (Priority: P1) 🎯 MVP

**Goal**: `RpcServer` depends on `IChainReader` for queries and `IChainWriter` for publish/createStream — not `IBlockchain`

**Independent Test**: `RpcServer` compiles and passes all RPC tests with narrow interface types; write methods are inaccessible through the read interface

### Implementation for User Story 1

- [X] T011 [US1] Update src/network/SessionHandler.hpp to accept dual-reference pattern (`const IChainReader&` + `IChainWriter&`) instead of requiring `IBlockchain&`
- [X] T012 [US1] Change src/network/RpcServer.hpp constructor and factory to accept `const IChainReader&` + `IChainWriter&` instead of `IBlockchain&`
- [X] T013 [US1] Update src/network/RpcServer.cpp to use reader reference for all query handlers and writer reference for handle_publish/handle_createStream
- [X] T014 [US1] Update RpcServer creation in src/main.cpp to pass narrow interfaces from the `Blockchain` instance
- [X] T015 [US1] Build with `make -j8` and run `./tests/rpc_expansion_tests` and `./tests/rpc_integration_tests` to verify no regressions

**Checkpoint**: RpcServer uses narrow interfaces. All RPC tests pass.

---

## Phase 4: User Story 2 — Network Layer Submits Blocks Through a Service Boundary (Priority: P2)

**Goal**: `ChainService` mediates between network components and domain layer. Network code never calls `appendBlock()`/`saveChunk()`/`saveKeys()` directly. Wire format drops `chunk_index`.

**Independent Test**: Network components compile with `ChainService&` injected; sync and propagation tests pass; wire format contains `start_index` not `chunk_index`

### Implementation for User Story 2

- [X] T016 [US2] Create src/ChainService.hpp with `submitBlock()`, `submitSyncBatch()`, `getChainHeight()`, `getBlockAtTip()`, `getConsensusConfig()` per contracts/ChainService.md
- [X] T017 [US2] Create src/ChainService.cpp implementing the validate-append-persist workflow per contracts/ChainService.md behavioral contract
- [X] T002 [US2] Add ChainService.cpp to src/Makefile.am libblockchain_core_a_SOURCES list (after src file exists)
- [X] T018 [US2] Write unit tests for ChainService in tests/chain_service_tests.cpp: test submitBlock validates then persists, submitSyncBatch handles overlap and appends new blocks, submitSyncBatch throws ValidationError on fork detection
- [X] T003 [US2] Add chain_service_tests target to tests/Makefile.am with sources and link flags (after test file exists)
- [X] T019 [US2] Replace `chunk_index` with `start_index` in SyncResponse struct in src/network/SyncMessages.hpp per contracts/SyncMessages.md
- [X] T020 [US2] Update src/network/PeerServer.hpp and src/network/PeerServer.cpp: replace `bc.chunkSize` references with block-index-range batching, use `IChainReader` for reads, set `response.start_index` instead of `response.chunk_index`
- [X] T021 [US2] Update src/network/PeerClient.hpp to accept `ChainService&` for mutation operations (in addition to `IChainReader` for reads)
- [X] T022 [US2] Update src/network/PeerClient.cpp `handle_sync_response()`: replace direct `appendBlock()`/`saveChunk()`/`saveKeys()` calls with `chain_service.submitSyncBatch()`; remove `chunk_index` usage from response processing
- [X] T023 [US2] Update src/BlockPropagation.hpp to accept `ChainService&` instead of `IBlockchain&` for mutation operations
- [X] T024 [US2] Update src/BlockPropagation.cpp `appendReceivedBlock()`: replace direct `appendBlock()`/`saveChunk()`/`saveKeys()` calls with `chain_service.submitBlock()`
- [X] T025 [US2] Wire ChainService in src/main.cpp: create `ChainService` after `Blockchain`, inject into `PeerManager`/`BlockPropagation`/`Server<PeerServer>` constructors
- [X] T026 [US2] Build with `make -j8` and run `./tests/chain_service_tests`, `./tests/block_propagation_tests`, `./tests/block_propagation_integration_tests`, `./tests/sync_tests`, `./tests/p2p_sync_integration_tests` to verify no regressions

**Checkpoint**: All network components use ChainService. Zero direct calls to appendBlock/saveChunk/saveKeys from network code. Wire format uses start_index.

---

## Phase 5: User Story 3 — Consistent Error Reporting Across All Operations (Priority: P3)

**Goal**: All operations use domain-specific exceptions. No bool-return failure patterns or log-and-continue patterns remain.

**Independent Test**: `PeerManager` methods throw `PeerError` on failure; callers catch exceptions instead of checking booleans; all tests pass

### Implementation for User Story 3

- [X] T027 [US3] Convert `PeerManager::add_peer()` in src/PeerManager.hpp and src/PeerManager.cpp from `bool` return to `void` with `throw PeerError(...)` on capacity exceeded
- [X] T028 [US3] Convert `PeerManager::remove_peer()` in src/PeerManager.hpp and src/PeerManager.cpp from `bool` return to `void` with `throw PeerError(...)` when peer not found
- [X] T029 [US3] Update all callers of `add_peer()` and `remove_peer()` across src/ to use try/catch instead of checking bool return values; add try/catch around the `remove_peer()` call inside `ban_peer()` (which legitimately bans an already-removed peer) and review `unban_peer()` for silent-failure patterns
- [X] T030 [US3] Update tests/peer_manager_tests.cpp and tests/peer_discovery_tests.cpp to expect exceptions instead of false returns
- [X] T031 [US3] Build with `make -j8` and run `./tests/blockchain_tests`, `./tests/lifecycle_tests`, `./tests/lifecycle_integration_tests` to verify no regressions

**Checkpoint**: All failure reporting uses exceptions. No bool-return failure patterns remain in the codebase.

---

## Phase 6: User Story 4 — Chain Replacement Operates Within Bounded Memory (Priority: P3)

**Goal**: `replaceChain()` processes candidates in 100-block batches with crash-safe rollback. Peak memory bounded by batch size.

**Independent Test**: `replaceChain()` writes to temp files during validation, atomically commits on success, preserves original chain on failure. Existing `chunk_replace_tests` pass with streaming implementation.

### Implementation for User Story 4

- [X] T032 [US4] Refactor `replaceChain()` signature in src/IChainWriter.hpp: add `virtual void replaceChainStreaming(size_t candidateLength, std::function<std::vector<Block>(size_t batchStart, size_t batchSize)> fetcher) = 0` and deprecate the existing `replaceChain(const std::vector<Block>&)` overload
- [X] T033 [US4] Implement streaming `replaceChain()` in src/Blockchain.cpp: clear difficulty cache at start (FR-011), validate batches of 100 blocks, write to temp chunk files, atomic rename on success, delete temp files on failure
- [X] T034 [US4] Update `ChainService::submitSyncBatch()` and any callers of `replaceChain()` in src/ to use the new streaming signature
- [X] T035 [US4] Update tests/chunk_replace_tests.cpp to verify: bounded-memory batch processing, original chain preserved on validation failure, difficulty cache cleared at start, end state identical to all-in-memory approach
- [X] T036 [US4] Build with `make -j8` and run `./tests/chunk_replace_tests`, `./tests/consensus_tests`, `./tests/sync_tests` to verify no regressions

**Checkpoint**: Chain replacement uses bounded memory with crash-safe rollback. All chain replacement tests pass.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final validation, documentation, and cleanup

- [X] T037 [P] Update docs/AUDIT.md to mark §6.1, §6.2, §6.3, and §4.5 as RESOLVED with brief descriptions of what was done
- [X] T038 [P] Update docs/ROADMAP.md to move 021-architecture-remediation to Completed table
- [X] T039 Full build with `make -j8` and run ALL test binaries individually per quickstart.md to verify zero regressions across the entire test suite

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 (ChainError.hpp must exist before Blockchain.cpp can use it)
- **US1 (Phase 3)**: Depends on Phase 2 (widened IChainReader must be in place)
- **US2 (Phase 4)**: Depends on Phase 2 (ChainError.hpp + widened IChainReader). Can run in parallel with US1.
- **US3 (Phase 5)**: Depends on Phase 2 (ChainError.hpp for PeerError). Can run in parallel with US1 and US2.
- **US4 (Phase 6)**: Depends on Phase 2 (exception types) and Phase 4 (ChainService must exist before streaming replaceChain callers update)
- **Polish (Phase 7)**: Depends on all user stories complete

### User Story Dependencies

- **US1 (P1)**: Independent after Phase 2. No dependencies on other stories.
- **US2 (P2)**: Independent after Phase 2. No dependencies on US1 or US3.
- **US3 (P3)**: Independent after Phase 2. No dependencies on US1 or US2.
- **US4 (P3)**: Depends on US2 (ChainService). Cannot start until Phase 4 is complete.

### Within Each User Story

- Interface/header changes before implementation changes
- Implementation before test updates
- Build verification as final step

### Parallel Opportunities

Within Phase 1: T001, T004 can run in parallel (different files).
Within Phase 4: T002 follows T017 (needs source file); T003 follows T018 (needs test file).
Within Phase 7: T037, T038 can run in parallel (different docs).
Across phases: US1 (Phase 3), US2 (Phase 4), US3 (Phase 5) can all start in parallel once Phase 2 completes.

---

## Parallel Example: After Foundational Phase

```bash
# These three user stories can proceed in parallel after Phase 2:
# Worker A: US1 — RpcServer interface narrowing (T011-T015)
# Worker B: US2 — ChainService + network refactor (T016-T026)
# Worker C: US3 — PeerManager exception conversion (T027-T031)

# US4 waits for US2 to complete, then proceeds:
# Worker A or B: US4 — Streaming replaceChain (T032-T036)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001-T004)
2. Complete Phase 2: Foundational (T005-T010)
3. Complete Phase 3: User Story 1 (T011-T015)
4. **STOP and VALIDATE**: RpcServer uses narrow interfaces, all RPC tests pass
5. This alone addresses audit §6.1

### Incremental Delivery

1. Setup + Foundational → Exception hierarchy + widened interfaces ready
2. Add US1 → Narrow RpcServer interfaces → Audit §6.1 addressed
3. Add US2 → ChainService mediates network → Audit §6.2 addressed
4. Add US3 → All exceptions standardized → Audit §6.3 addressed
5. Add US4 → Streaming replaceChain → Audit §4.5 addressed
6. Polish → AUDIT.md + ROADMAP.md updated → Feature complete
