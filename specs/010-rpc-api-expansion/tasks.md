# Tasks: RPC API Expansion

**Input**: Design documents from `/specs/010-rpc-api-expansion/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: Interface change required by all user stories

- [ ] T001 Add `virtual uint32_t getCurrentDifficulty() const = 0` to the IBlockchain interface in src/IBlockchain.hpp
- [ ] T002 Add `override` keyword to `getCurrentDifficulty()` in src/Blockchain.hpp
- [ ] T003 Add `getCurrentDifficulty()` stub returning a default value to MockBlockchain in tests/MockBlockchain.hpp

---

## Phase 2: Foundational (Build & Test Infrastructure)

**Purpose**: Register new test binary so all user story tests can compile and run via `make check`

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T004 Add `rpc_expansion_tests` binary to tests/Makefile.am with sources matching existing test binary pattern (link RpcServer.cpp, Block.cpp, Blockchain.cpp, Chunk.cpp, utils.cpp, NodeConfig.cpp, PeerManager.cpp, BlockPropagation.cpp, MerkleTree.cpp, PeerClient.cpp, PeerServer.cpp)
- [ ] T005 [P] Create empty test scaffold in tests/rpc_expansion_tests.cpp with Catch2 `#define CATCH_CONFIG_MAIN` and include headers for MockBlockchain, RpcServer, SyncState, PeerManager
- [ ] T006 [P] Add `tests/rpc_expansion_tests` to .gitignore
- [ ] T007 [P] [CLEANUP] Add `tests/cli_tests` to .gitignore (missing from 009 implementation per constitution sync report)

**Checkpoint**: `make check` compiles and runs the new (empty) test binary

---

## Phase 3: User Story 1 — Operator Checks Node Health (Priority: P1) 🎯 MVP

**Goal**: Operator calls `getNodeStatus` and receives chain length, chunk count, sync state, current difficulty, peer counts, and node UUID in a single response.

**Independent Test**: Start a node (via MockBlockchain), call `getNodeStatus`, verify all 7 fields are present and correct.

### Tests for User Story 1

- [ ] T008 [US1] Write Catch2 unit and integration tests for `getNodeStatus` in tests/rpc_expansion_tests.cpp: test response contains all 7 fields (chainLength, chunkCount, syncState, currentDifficulty, inboundPeers, outboundPeers, nodeUuid), test with sync active shows "syncing", test with no peer_manager shows zero peer counts. Integration coverage: tests exercise the full JSON-RPC dispatch path (JSON parse → method match → handler → response serialization) via MockBlockchain to satisfy constitution §III

### Implementation for User Story 1

- [ ] T009 [US1] Implement `getNodeStatus` handler in src/network/RpcServer.cpp: assemble JSON object from bc.getChainLength(), bc.getChunkCount(), bc.getCurrentDifficulty(), sync_status->isSyncing, peer_manager counts and UUID; use resultJsonMessage() for response; default peer counts to 0 when peer_manager is null

**Checkpoint**: `getNodeStatus` returns complete health snapshot; tests pass via `make check`

---

## Phase 4: User Story 2 — Client Fetches a Range of Blocks (Priority: P2)

**Goal**: Client calls `getBlockRange` with startIndex, endIndex, and optional headersOnly to retrieve up to 1000 contiguous blocks in one request.

**Independent Test**: Populate MockBlockchain with multiple blocks, request ranges, verify correct blocks returned in order; verify all error conditions.

### Tests for User Story 2

- [ ] T010 [US2] Write Catch2 unit and integration tests for `getBlockRange` in tests/rpc_expansion_tests.cpp: valid range returns correct blocks in order, end index clamped when beyond chain length, headersOnly=true returns header-only objects, start > end returns error -32602, start beyond chain returns error -32001, range exceeding 1000 returns error -32602, missing params returns error -32602, start=0 end=0 returns genesis block only. Integration coverage: tests exercise the full JSON-RPC dispatch path (JSON parse → method match → handler → response serialization) via MockBlockchain to satisfy constitution §III

### Implementation for User Story 2

- [ ] T011 [US2] Implement `getBlockRange` handler in src/network/RpcServer.cpp: validate params (startIndex, endIndex required, headersOnly optional bool default false), enforce static constexpr kMaxBlockRange = 1000, validate start <= end (-32602), validate start < chainLength (-32001), validate range size (-32602), clamp endIndex to chainLength-1, iterate and collect blocks via bc.getBlockByIndex() using toJson() or toHeaderJson() based on headersOnly flag, return array via resultMessage() with dump()

**Checkpoint**: `getBlockRange` handles all valid and error cases; tests pass via `make check`

---

## Phase 5: User Story 3 — Operator Queries Chain Metrics Individually (Priority: P3)

**Goal**: Operator calls `getChainLength` or `getChunkCount` to get a single integer metric for monitoring scripts and dashboards.

**Independent Test**: Call each method on a MockBlockchain with known block count, verify integer response matches.

### Tests for User Story 3

- [ ] T012 [P] [US3] Write Catch2 tests for `getChainLength` in tests/rpc_expansion_tests.cpp: returns correct integer for chain with multiple blocks, returns 1 for genesis-only chain

- [ ] T013 [P] [US3] Write Catch2 tests for `getChunkCount` in tests/rpc_expansion_tests.cpp: returns correct chunk count, returns 1 for genesis-only chain

### Implementation for User Story 3

- [ ] T014 [P] [US3] Implement `getChainLength` handler in src/network/RpcServer.cpp: call bc.getChainLength(), return integer via resultMessage() with std::to_string()
- [ ] T015 [P] [US3] Implement `getChunkCount` handler in src/network/RpcServer.cpp: call bc.getChunkCount(), return integer via resultMessage() with std::to_string()

**Checkpoint**: Both metric endpoints return correct integers; tests pass via `make check`

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Final validation, documentation, and constitution compliance

- [ ] T016 Run `make check` to verify all existing and new tests pass
- [ ] T017 Run quickstart.md validation steps against a running node to verify all 4 new endpoints work end-to-end over TLS
- [ ] T018 Update docs/ROADMAP.md: move spec 010 from "Suggested Specs" to "Completed" table with summary and updated date (constitution Principle XIII)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 completion — BLOCKS all user stories
- **Phase 3 (US1)**: Depends on Phase 2 completion
- **Phase 4 (US2)**: Depends on Phase 2 completion; independent from US1
- **Phase 5 (US3)**: Depends on Phase 2 completion; independent from US1 and US2
- **Phase 6 (Polish)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Independent — requires only IBlockchain interface change + test infrastructure
- **User Story 2 (P2)**: Independent — requires only IBlockchain + test infrastructure
- **User Story 3 (P3)**: Independent — requires only IBlockchain + test infrastructure

### Within Each User Story

- Tests written first, verified to fail or be pending
- Implementation follows, tests verified to pass
- All work within a single source file (RpcServer.cpp) + single test file

### Parallel Opportunities

- T005, T006, T007 can all run in parallel (different files, no dependencies)
- After Phase 2, US1/US2/US3 can proceed in parallel (each touches different RPC methods)
- T012, T013 can run in parallel (different test sections)
- T014, T015 can run in parallel (different if-else branches in same file, but independent logic)

---

## Parallel Example: After Phase 2

```
# All three user stories can proceed in parallel:
Stream A: T008 → T009            (US1: getNodeStatus)
Stream B: T010 → T011            (US2: getBlockRange)
Stream C: T012+T013 → T014+T015  (US3: getChainLength + getChunkCount)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Interface change (T001–T003)
2. Complete Phase 2: Test infrastructure (T004–T007)
3. Complete Phase 3: getNodeStatus (T008–T009)
4. **STOP and VALIDATE**: `make check` passes, getNodeStatus works
5. Operator has immediate node visibility

### Incremental Delivery

1. Phase 1 + 2 → Foundation ready
2. Add US1 (getNodeStatus) → Operator health check available (MVP!)
3. Add US2 (getBlockRange) → Block explorers can batch-query
4. Add US3 (getChainLength + getChunkCount) → Monitoring scripts work
5. Phase 6 → Full validation and roadmap update
