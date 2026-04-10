# Tasks: Consensus Mechanism

**Input**: Design documents from `/specs/002-consensus-mechanism/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

**Tests**: Included — constitution §III requires full test coverage for every new feature.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3, US4)
- Include exact file paths in descriptions

## Phase 1: Setup

**Purpose**: Build system updates for new source and test files

- [X] T001 Add `ConsensusConfig.hpp` and `consensus_tests.cpp` to build system in `src/Makefile.am` and `tests/Makefile.am`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core data structures and utilities that ALL user stories depend on

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T002 [P] Create `ConsensusConfig` struct with env-var loading in `src/ConsensusConfig.hpp` — fields: `targetBlockInterval`, `adjustmentWindow`, `maxAdjustmentFactor`, `minDifficulty`, `maxDifficulty`, `initialDifficulty`, `miningTimeout`, `maxFutureTimestamp`, `maxReorgDepth` with defaults per data-model.md; read from environment variables; load via `loadDotEnv` in `src/main.cpp`
- [X] T003 [P] Add `nonce` (`uint64_t`, default 0) and `difficulty` (`uint32_t`, default 0) fields to `Block` struct in `src/Block.hpp` — update `serialize()` to include both fields unconditionally; update default constructor to initialize them
- [X] T004 Update `Block::calculateHash()` in `src/Block.cpp` to include `nonce` and `difficulty` in the hash input string: `hash = SHA256(index || timestamp || data || prevHash || nonce || difficulty)`
- [X] T005 Update `Block` constructor in `src/Block.cpp` to accept `nonce` and `difficulty` parameters; keep existing constructor signature working by defaulting both to 0
- [X] T006 [P] Add `checkLeadingZeroBits(const std::string& hashStr, uint32_t bitsNeeded) -> bool` function in `src/utils.hpp` and `src/utils.cpp` — iterate hex chars counting zero digits (4 bits each), then check partial bits in first non-zero nibble per research.md R2 algorithm
- [X] T007 [P] Update `Block::toJson()` in `src/Block.cpp` to include `nonce` and `difficulty` fields in the returned JSON object
- [X] T008 Update existing block tests in `tests/block_tests.cpp` — fix any tests broken by the new `calculateHash()` that now includes nonce/difficulty; verify Block construction still works with default nonce=0, difficulty=0

**Checkpoint**: Block struct extended, ConsensusConfig available, checkLeadingZeroBits utility ready. User story implementation can begin.

---

## Phase 3: User Story 1 — Node Validates Blocks Before Acceptance (Priority: P1) 🎯 MVP

**Goal**: Incoming blocks are validated against consensus rules (PoW, timestamp, difficulty) before being accepted into the chain.

**Independent Test**: Submit blocks with varying difficulty proofs to a single node and verify only blocks meeting the difficulty target are accepted.

### Tests for User Story 1

- [X] T009 [P] [US1] Write unit tests in `tests/consensus_tests.cpp` for `isValidNewBlock` consensus validation: test that a block with valid PoW is accepted; test that a block with invalid PoW (hash doesn't meet difficulty) is rejected; test that a block with incorrect prevHash is rejected; test that genesis block is exempt from PoW; test that a block with future timestamp beyond 120s is rejected; test that `checkLeadingZeroBits` correctly identifies valid/invalid hashes at various difficulty levels; test that a block with `difficulty < config.minDifficulty` is rejected

### Implementation for User Story 1

- [X] T010 [US1] Extend `IBlockchain::isValidNewBlock()` in `src/IBlockchain.hpp` to add consensus validation: verify `checkLeadingZeroBits(newBlock.hash, newBlock.difficulty)` is true; verify `newBlock.difficulty >= config.minDifficulty` (full height-based difficulty verification is added in T022 after `getDifficultyForHeight` exists); verify `newBlock.timestamp <= now + config.maxFutureTimestamp`; exempt genesis block (index 0) from PoW checks; return descriptive error messages via `logMessage()` for each failure type per FR-012
- [X] T011 [US1] Update `generateGenesisBlock()` in `src/Blockchain.cpp` to set `nonce=0, difficulty=0` on the genesis block explicitly
- [X] T012 [US1] Pass `ConsensusConfig` to `IBlockchain` and `Blockchain` — add a `ConsensusConfig` member to `IBlockchain` (or pass by reference); update constructor in `src/Blockchain.hpp` and `src/Blockchain.cpp`; update `main.cpp` to construct `ConsensusConfig` from env vars and pass to `Blockchain`

**Checkpoint**: Blocks are validated against PoW, timestamp, and difficulty rules. Invalid blocks are rejected with descriptive errors. Tests pass via `make check`.

---

## Phase 4: User Story 2 — Node Mines Blocks with Proof-of-Work (Priority: P1)

**Goal**: `addBlock` computes a valid proof-of-work nonce before appending the block, with a configurable mining timeout.

**Independent Test**: Call `addBlock` and verify the returned block contains a valid nonce and hash meeting the current difficulty target.

### Tests for User Story 2

- [X] T013 [P] [US2] Write unit tests in `tests/consensus_tests.cpp` for mining: test that `addBlock` returns a block with valid PoW at difficulty 1; test that the returned block's hash has the required leading zero bits; test that the nonce is non-zero (for difficulty >= 1); test mining timeout returns an error when difficulty is impossibly high (set difficulty > 200 bits for test)

### Implementation for User Story 2

- [X] T014 [US2] Implement the mining loop in `Blockchain::addBlock()` in `src/Blockchain.cpp` — after constructing the candidate block, loop: set `nonce`, call `calculateHash()`, check `checkLeadingZeroBits(hash, currentDifficulty)`; increment nonce until valid or timeout exceeded; use `std::chrono::steady_clock` for timeout tracking against `config.miningTimeout`; on timeout throw or return error per FR-013; set block's `difficulty` field to `currentDifficulty` before mining starts
- [X] T015 [US2] Update `RpcServer::do_read()` in `src/network/RpcServer.cpp` — handle mining timeout error from `addBlock`: catch the error and return JSON-RPC error code -32000 with message "Mining timeout exceeded ({N}s)" per contracts/json-rpc.md

**Checkpoint**: `addBlock` mines blocks with valid PoW. RPC clients receive mined blocks with nonce/difficulty fields. Mining timeout produces a clean JSON-RPC error. Tests pass.

---

## Phase 5: User Story 3 — Network Resolves Competing Chains (Priority: P2)

**Goal**: When presented with a competing chain, the node adopts the longest valid chain (within max reorg depth).

**Independent Test**: Create two blockchain instances with divergent chains of different lengths; verify the shorter chain is replaced by the longer valid chain.

### Tests for User Story 3

- [X] T016 [P] [US3] Write unit tests in `tests/consensus_tests.cpp` for chain replacement: test that a longer valid chain replaces a shorter chain; test that a shorter chain does not replace a longer chain; test that a longer chain with an invalid block is rejected entirely; test that a chain requiring reorganization deeper than `maxReorgDepth` is rejected; test that `keyIndexMap` is correctly rebuilt after chain replacement

### Implementation for User Story 3

- [X] T017 [US3] Implement `isValidChain()` method in `src/Blockchain.hpp` and `src/Blockchain.cpp` — validate an entire chain from genesis: verify genesis block structure, then iterate and call `isValidNewBlock()` for each consecutive pair; return false on first invalid block
- [X] T018 [US3] Implement `replaceChain()` method in `src/Blockchain.hpp` and `src/Blockchain.cpp` — accept a candidate chain (vector of ChunkHandlers); check candidate length > current length; check reorg depth <= `config.maxReorgDepth`; call `isValidChain()` on candidate; if valid: free current chunks, replace `chain` vector, rebuild `keyIndexMap`; log descriptive messages for rejection reasons per FR-012
- [X] T019 [US3] Add `replaceChain` to `IBlockchain` interface in `src/IBlockchain.hpp` as a virtual method so the network layer can call it on received chains

**Checkpoint**: Chain replacement works with longest-valid-chain rule, max reorg depth enforced, keyIndexMap rebuilt. Tests pass.

---

## Phase 6: User Story 4 — Difficulty Adjusts Over Time (Priority: P3)

**Goal**: Mining difficulty adjusts automatically every N blocks based on block production rate.

**Independent Test**: Mine blocks with artificially fast/slow timestamps and verify difficulty increases/decreases accordingly.

### Tests for User Story 4

- [X] T020 [P] [US4] Write unit tests in `tests/consensus_tests.cpp` for difficulty adjustment: test difficulty increases when blocks are mined faster than target interval; test difficulty decreases when blocks are mined slower than target interval; test difficulty does not change by more than `maxAdjustmentFactor`; test difficulty is clamped to `[minDifficulty, maxDifficulty]` range; test adjustment triggers every `adjustmentWindow` blocks; test difficulty stays at minimum when blocks are slow and already at minimum

### Implementation for User Story 4

- [X] T021 [US4] Implement `calculateNewDifficulty()` in `src/Blockchain.hpp` and `src/Blockchain.cpp` — per research.md R3 algorithm: compute `expectedTime = targetInterval * windowSize`; compute `actualTime` from timestamps of first and last block in window; `ratio = expectedTime / actualTime`; clamp ratio to `[1/maxFactor, maxFactor]`; `newDifficulty = current + round(log2(ratio))`; clamp to `[minDifficulty, maxDifficulty]`
- [X] T022 [US4] Implement `getDifficultyForHeight()` in `src/Blockchain.hpp` and `src/Blockchain.cpp` — given a block height, compute what the difficulty should be by checking if the height falls on an adjustment boundary; retrofit `isValidNewBlock()` (from T010) to call `getDifficultyForHeight()` for exact height-based difficulty verification, replacing the initial range check
- [X] T023 [US4] Integrate difficulty adjustment into `addBlock()` in `src/Blockchain.cpp` — after appending a mined block, check if `blockIndex % config.adjustmentWindow == 0`; if so, call `calculateNewDifficulty()` and update `currentDifficulty` for the next block

**Checkpoint**: Difficulty adjusts every N blocks. Fast mining increases difficulty; slow mining decreases it. Clamping prevents wild oscillation. Tests pass.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final integration verification and build validation

- [X] T024 Verify `make check` passes all tests (existing block_tests + new consensus_tests) — run full test suite and fix any regressions
- [X] T025 Run quickstart.md validation — start the node, call `addBlock` via RPC, verify response includes `nonce` and `difficulty` fields, verify mining completes successfully

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS all user stories
- **US1 Validation (Phase 3)**: Depends on Foundational
- **US2 Mining (Phase 4)**: Depends on Foundational + US1 (mining calls `isValidNewBlock`)
- **US3 Chain Replacement (Phase 5)**: Depends on Foundational + US1 (replacement validates entire chain)
- **US4 Difficulty Adjustment (Phase 6)**: Depends on Foundational + US2 (adjustment triggers after mining)
- **Polish (Phase 7)**: Depends on all user stories

### User Story Dependencies

- **US1 (P1)**: Depends on Foundational only — can start immediately after Phase 2
- **US2 (P1)**: Depends on US1 — mining must validate the mined block
- **US3 (P2)**: Depends on US1 — chain replacement validates every block
- **US4 (P3)**: Depends on US2 — difficulty adjustment is triggered by addBlock; T022 retrofits T010 to use `getDifficultyForHeight` for exact difficulty validation

### Within Each User Story

- Tests MUST be written and FAIL before implementation (constitution §III)
- Data structure changes before logic
- Core logic before integration points (RPC, network)

### Parallel Opportunities

**Within Phase 2** (all independent files):
- T002 (ConsensusConfig.hpp) ‖ T003 (Block.hpp fields) ‖ T006 (utils checkLeadingZeroBits) ‖ T007 (Block::toJson)

**Across User Stories** (if team capacity/separate PRs):
- US3 can start in parallel with US2 (both depend only on US1). Note: US3 chain validation initially uses the pre-US4 `isValidNewBlock` (range-check only for difficulty). After US4 completes, chain validation automatically gains full difficulty-schedule verification.

---

## Implementation Strategy

### MVP Scope

**User Story 1 (Validation)** is the MVP. After completing Phases 1–3:
- Blocks are validated against PoW rules
- Invalid blocks are rejected with descriptive errors
- The node enforces consensus rules even without mining (blocks can be manually constructed with valid PoW for testing)

### Incremental Delivery

1. **Phase 1–3** → MVP: Validation works, consensus rules are enforced
2. **Phase 4** → Full local operation: `addBlock` mines with PoW, timeout safety
3. **Phase 5** → Multi-node readiness: Chain replacement enables convergence (wired to P2P in spec 003/005)
4. **Phase 6** → Long-term viability: Difficulty adjusts to network conditions
5. **Phase 7** → Polish: Full test suite green, quickstart verified
