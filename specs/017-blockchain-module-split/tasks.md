# Tasks: Blockchain Module Split

**Input**: Design documents from `/specs/017-blockchain-module-split/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: Build system preparation and skeleton files for all new modules

- [x] T001 Add ChainPersistence.cpp, DifficultyEngine.cpp, MerkleProofService.cpp to libblockchain_core_a_SOURCES in src/Makefile.am
- [x] T002 [P] Create src/ChainPersistence.hpp with template class declaration, constructor, and all public method signatures per contracts/chain-persistence.md
- [x] T003 [P] Create src/DifficultyEngine.hpp with class declaration, default constructor, and public method signatures per contracts/difficulty-engine.md
- [x] T004 [P] Create src/MerkleProofService.hpp with class declaration, default constructor, and public method signatures per contracts/merkle-proof-service.md
- [x] T005 [P] Create src/ChainPersistence.cpp with empty stub implementations and explicit template instantiation for Chunk and MockChunk
- [x] T006 [P] Create src/DifficultyEngine.cpp with empty stub implementations
- [x] T007 [P] Create src/MerkleProofService.cpp with empty stub implementations
- [x] T008 Verify project compiles with `make -j8` (stubs only, no logic yet)

**Checkpoint**: All new files exist, compile cleanly, and link into libblockchain_core.a

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Update Blockchain.hpp to own module instances; verify existing tests still pass before any logic is moved

**⚠️ CRITICAL**: No module extraction can begin until Blockchain.hpp correctly owns and constructs all three module members

- [x] T009 Add #include directives for ChainPersistence.hpp, DifficultyEngine.hpp, MerkleProofService.hpp in src/Blockchain.hpp
- [x] T010 Add persistence_, difficultyEngine_, and proofService_ member fields to the Blockchain template class in src/Blockchain.hpp
- [x] T011 Initialize persistence_ with blockchainPath and chunkSize in Blockchain constructor in src/Blockchain.hpp
- [x] T012 Verify project compiles with `make -j8` and all existing test binaries pass

**Checkpoint**: Blockchain owns all three module members; zero test regressions

---

## Phase 3: User Story 1 — Independent Module Maintenance (Priority: P1) 🎯 MVP

**Goal**: Extract all persistence logic from Blockchain.cpp into ChainPersistence, enabling independent maintenance of persistence code

**Independent Test**: Modify only ChainPersistence.cpp, rebuild with `make -j8`, confirm only ChainPersistence.o is recompiled and all tests pass

### Implementation for User Story 1

- [x] T013 [US1] Move saveChunk() logic from src/Blockchain.cpp into ChainPersistence::saveChunk() in src/ChainPersistence.cpp
- [x] T014 [US1] Move loadChunk() logic from src/Blockchain.cpp into ChainPersistence::loadChunk() in src/ChainPersistence.cpp
- [x] T015 [US1] Move freeChunk() logic from src/Blockchain.cpp into ChainPersistence::freeChunk() in src/ChainPersistence.cpp
- [x] T016 [US1] Move saveKeys() and loadKeys() logic from src/Blockchain.cpp into src/ChainPersistence.cpp
- [x] T017 [US1] Move saveStreams() and loadStreams() logic from src/Blockchain.cpp into src/ChainPersistence.cpp
- [x] T018 [US1] Move saveStreamIndex() and loadStreamIndex() logic from src/Blockchain.cpp into src/ChainPersistence.cpp
- [x] T019 [US1] Move saveAllChunks() logic from src/Blockchain.cpp into ChainPersistence::saveAllChunks() in src/ChainPersistence.cpp
- [x] T020 [US1] Move discoverChunks() and validateChunk() logic from src/Blockchain.cpp into src/ChainPersistence.cpp
- [x] T021 [US1] Move recoverChain() logic from src/Blockchain.cpp into ChainPersistence::recoverChain() in src/ChainPersistence.cpp
- [x] T022 [US1] Move archiveChainFiles() logic from src/Blockchain.cpp into ChainPersistence::archiveChainFiles() in src/ChainPersistence.cpp
- [x] T023 [US1] Replace all moved persistence method bodies in src/Blockchain.cpp with delegation calls to persistence_ member
- [x] T024 [US1] Verify project compiles with `make -j8` and all existing test binaries pass with zero failures
- [x] T025 [US1] Add focused persistence tests in tests/chain_persistence_tests.cpp — test saveChunk/loadChunk round-trip, saveAllChunks clears dirty flag, recoverChain rebuilds indexes
- [x] T026 [US1] Add chain_persistence_tests binary to tests/Makefile.am and verify it passes

**Checkpoint**: All persistence logic lives in ChainPersistence; Blockchain.cpp delegates; all tests pass; SC-001 persistence module ≤400 lines

---

## Phase 4: User Story 2 — Isolated Difficulty Adjustment Changes (Priority: P2)

**Goal**: Extract difficulty calculation and caching logic from Blockchain.cpp into DifficultyEngine, enabling isolated difficulty changes

**Independent Test**: Modify only DifficultyEngine.cpp, rebuild, confirm only DifficultyEngine.o is recompiled and all tests pass

### Implementation for User Story 2

- [x] T027 [US2] Move calculateNewDifficulty() logic from src/Blockchain.cpp into DifficultyEngine::calculateNewDifficulty() in src/DifficultyEngine.cpp
- [x] T028 [US2] Move getDifficultyForHeight() logic from src/Blockchain.cpp into DifficultyEngine::getDifficultyForHeight() in src/DifficultyEngine.cpp
- [x] T029 [US2] Replace difficulty method bodies in src/Blockchain.cpp with delegation calls to difficultyEngine_ member, passing config, totalBlockCount_, currentDifficulty, difficultyCache_, and block accessor/retainer callbacks
- [x] T030 [US2] Verify project compiles with `make -j8` and all existing test binaries pass with zero failures
- [x] T031 [US2] Add focused difficulty tests in tests/difficulty_engine_tests.cpp — test calculateNewDifficulty with mock blocks below/at/above adjustment window, getDifficultyForHeight cache hit/miss, clamping to min/max difficulty
- [x] T032 [US2] Add difficulty_engine_tests binary to tests/Makefile.am and verify it passes

**Checkpoint**: All difficulty logic lives in DifficultyEngine; Blockchain.cpp delegates; all tests pass; SC-001 difficulty module ≤400 lines

---

## Phase 5: User Story 3 — Standalone Merkle Proof Testing (Priority: P3)

**Goal**: Extract Merkle proof logic from Blockchain.cpp into MerkleProofService, enabling standalone proof testing and security review

**Independent Test**: Run merkle_proof_tests binary with in-memory block data; no disk I/O or chain state needed

### Implementation for User Story 3

- [x] T033 [US3] Move getInclusionProof() logic from src/Blockchain.cpp into MerkleProofService::getInclusionProof() in src/MerkleProofService.cpp
- [x] T034 [US3] Move verifyInclusionProof() logic from src/Blockchain.cpp into MerkleProofService::verifyInclusionProof() in src/MerkleProofService.cpp
- [x] T035 [US3] Replace proof method bodies in src/Blockchain.cpp with delegation calls to proofService_ member, fetching the block and passing it to the service
- [x] T036 [US3] Verify project compiles with `make -j8` and all existing test binaries pass with zero failures
- [x] T037 [US3] Add focused proof tests in tests/merkle_proof_tests.cpp — test getInclusionProof with single/multiple entries, verifyInclusionProof valid/tampered proof, entry index out-of-bounds error
- [x] T038 [US3] Add merkle_proof_tests binary to tests/Makefile.am and verify it passes

**Checkpoint**: All proof logic lives in MerkleProofService; Blockchain.cpp delegates; all tests pass; SC-001 proof module ≤400 lines

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final validation, line-count enforcement, documentation, and roadmap update

- [x] T039 Verify no single source module exceeds 400 lines: `wc -l src/Blockchain.cpp src/ChainPersistence.cpp src/DifficultyEngine.cpp src/MerkleProofService.cpp` (SC-001)
- [x] T040 Run all existing test binaries individually and confirm zero failures (SC-002)
- [x] T041 [P] Update .gitignore with new test binary paths if needed (tests/chain_persistence_tests, tests/difficulty_engine_tests, tests/merkle_proof_tests)
- [x] T042 [P] Update docs/ROADMAP.md — move 017-blockchain-module-split to Completed table with summary
- [x] T043 Run quickstart.md validation — verify build and all test binaries pass per documented workflow
- [x] T044 Verify incremental build scope: touch src/ChainPersistence.cpp, run `make -j8`, confirm only ChainPersistence.o is recompiled (SC-003, US4)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 completion — BLOCKS all user stories
- **User Story 1 / Phase 3 (P1)**: Depends on Phase 2; no dependency on other stories
- **User Story 2 / Phase 4 (P2)**: Depends on Phase 2; no dependency on US1 or US3
- **User Story 3 / Phase 5 (P3)**: Depends on Phase 2; no dependency on US1 or US2
- **Polish (Phase 6)**: Depends on all user stories being complete

### User Story Dependencies

- **US1 (ChainPersistence)**: Independent after Phase 2 — largest extraction, recommended first
- **US2 (DifficultyEngine)**: Independent after Phase 2 — can run in parallel with US1/US3
- **US3 (MerkleProofService)**: Independent after Phase 2 — can run in parallel with US1/US2

### Integration Test Note

Constitution III requires unit and integration tests for new features. This feature adds 3 unit test binaries (T025, T031, T037). No new integration tests are added because this is a non-network internal refactoring — the existing integration tests (p2p_sync_integration_tests, block_propagation_integration_tests, lifecycle_integration_tests) exercise the full chain through the unchanged IBlockchain interface and validate end-to-end behavior post-split.

### FR-008 (Single-Threaded Guarantee)

FR-008 is preserved by design — no async, threaded, or concurrent code is introduced. All module methods are synchronous and called from the owning Blockchain instance within the existing single-threaded io_context model.

### Within Each User Story

- Move logic to new module first
- Replace Blockchain.cpp method bodies with delegation calls
- Compile and run existing tests (regression check)
- Add new focused tests
- Register new test binary in Makefile.am

### Parallel Opportunities

**Phase 1** (setup):
```
T002  T003  T004  — headers in parallel (different files)
T005  T006  T007  — implementations in parallel (different files)
```

**Phase 2** (foundational): Sequential — each step modifies Blockchain.hpp

**Phases 3–5** (user stories): All three phases can run in parallel since each extracts to a different module file. Within US1, T016/T017/T018 (index I/O pairs) can run in parallel.

**Phase 6** (polish): T041 and T042 can run in parallel.

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (skeleton files + build)
2. Complete Phase 2: Foundational (Blockchain owns modules)
3. Complete Phase 3: User Story 1 (ChainPersistence extraction)
4. **STOP and VALIDATE**: All existing tests pass; persistence code is isolated
5. This alone delivers the highest-value outcome from the audit

### Incremental Delivery

1. Setup + Foundational → Build compiles with module stubs
2. Add US1 (ChainPersistence) → Test independently → Persistence isolated (MVP!)
3. Add US2 (DifficultyEngine) → Test independently → Difficulty isolated
4. Add US3 (MerkleProofService) → Test independently → Proofs isolated
5. Polish → Line counts verified, roadmap updated, all tests green
6. Each story adds value without breaking previous stories

### User Story 4 (Reduced Build Times)

US4 is automatically satisfied by completing US1–US3. Once modules are in separate `.cpp` files, incremental builds only recompile the changed module. No additional tasks needed.
