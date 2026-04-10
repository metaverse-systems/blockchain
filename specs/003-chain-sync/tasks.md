# Tasks: Chain Synchronization

**Input**: Design documents from `/specs/003-chain-sync/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/json-rpc.md, contracts/p2p-binary.md, quickstart.md

**Tests**: Required per constitution Principle III (full Catch2 unit + integration tests with mocks).

**Organization**: Tasks grouped by user story. Each story can be implemented and tested independently.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: Wire format structs, shared sync state, and interface changes needed by all user stories

- [ ] T001 Define `SyncQuery` struct with Boost.Serialization in `src/network/SyncMessages.hpp`
- [ ] T002 [P] Define `SyncResponse` struct with Boost.Serialization in `src/network/SyncMessages.hpp`
- [ ] T003 [P] Define `SyncState` enum (`IDLE`, `SYNCING`) and add `std::atomic<bool> isSyncing` flag to a shared location accessible by `PeerClient` and `RpcServer` in `src/SyncState.hpp`
- [ ] T004 Promote `getChainBlockCount()` from `Blockchain` to `IBlockchain` interface in `src/IBlockchain.hpp`
- [ ] T005 Update `tests/Makefile.am` to add `sync_tests` target and link new test source file

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: PeerServer handler for BLOCKCHAIN_QUERY — must work before any client-side sync can be tested

**CRITICAL**: No user story work can begin until this phase is complete

- [ ] T006 Handle `BLOCKCHAIN_QUERY` in `PeerServer::do_read_body()` — add case to `switch(header.type)` dispatch, read `SyncQuery`, load requested chunks, send sequential `BLOCKCHAIN_RESPONSE` packets in `src/network/PeerServer.cpp`
- [ ] T007 Unit test: PeerServer responds to BLOCKCHAIN_QUERY with correct chunks in `tests/sync_tests.cpp`
- [ ] T008 [P] Unit test: PeerServer handles 3 concurrent BLOCKCHAIN_QUERY packets from separate connections without data corruption in `tests/sync_tests.cpp`

**Checkpoint**: A peer can now receive a sync query and respond with chunk data over the existing P2P TLS channel.

---

## Phase 3: User Story 1 — New Node Joins and Downloads Chain (Priority: P1) MVP

**Goal**: A new node with only a genesis block connects to a peer and downloads the full chain automatically.

**Independent Test**: Start a node with multiple blocks, connect a fresh node, verify the fresh node ends up with the same chain.

### Tests for User Story 1

- [ ] T009 [P] [US1] Unit test: PeerClient sends BLOCKCHAIN_QUERY with correct local chain height after connect in `tests/sync_tests.cpp`
- [ ] T010 [P] [US1] Unit test: PeerClient receives BLOCKCHAIN_RESPONSE, validates blocks, and persists chunk in `tests/sync_tests.cpp`
- [ ] T011 [P] [US1] Unit test: PeerClient transitions from IDLE to SYNCING on connect and back to IDLE on completion in `tests/sync_tests.cpp`

### Implementation for User Story 1

- [ ] T012 [US1] Add sync initiation logic to `PeerClient` — after TLS handshake completes, send `BLOCKCHAIN_QUERY` with local chain height in `src/network/PeerClient.hpp` and `src/network/PeerClient.cpp`
- [ ] T013 [US1] Add response handler to `PeerClient` — read `BLOCKCHAIN_RESPONSE` header + body, deserialize `SyncResponse` in `src/network/PeerClient.hpp` and `src/network/PeerClient.cpp`
- [ ] T014 [US1] Implement chunk validation in `PeerClient` response handler — validate each block in received chunk using `IBlockchain::isValidNewBlock()` in `src/network/PeerClient.cpp`
- [ ] T015 [US1] Implement chunk persistence in `PeerClient` response handler — on valid chunk, call `IBlockchain::saveChunk()` and request next chunk or transition to IDLE in `src/network/PeerClient.cpp`
- [ ] T016 [US1] Wire sync trigger in `main.cpp` — pass `IBlockchain` reference and `SyncState` to `PeerClient`, invoke sync after `connect()` handshake completes in `src/main.cpp`
- [ ] T017 [US1] Add sync progress logging — log chunk received, validation outcome, blocks synced count in `src/network/PeerClient.cpp`

**Checkpoint**: A fresh node connecting to a peer with blocks automatically downloads the full chain. `make check` passes.

---

## Phase 4: User Story 2 — Node Recovers After Downtime (Priority: P2)

**Goal**: A node that was offline catches up by downloading only the missing blocks (incremental sync).

**Independent Test**: Two nodes in sync, add blocks to one while the other is stopped, restart the stopped node — it downloads only missing blocks.

### Tests for User Story 2

- [ ] T018 [P] [US2] Unit test: PeerClient sends BLOCKCHAIN_QUERY with height > 1 and receives only missing chunks in `tests/sync_tests.cpp`
- [ ] T019 [P] [US2] Unit test: PeerServer sends only chunks after the requester's chain height in `tests/sync_tests.cpp`

### Implementation for User Story 2

- [ ] T020 [US2] Update PeerServer BLOCKCHAIN_QUERY handler to calculate `start_chunk` from `local_chain_height` and send only missing chunks in `src/network/PeerServer.cpp`
- [ ] T021 [US2] Update PeerClient to handle partial first chunk (when local height is not chunk-aligned) — append only missing blocks in `src/network/PeerClient.cpp`
- [ ] T022 [US2] Ensure PeerClient sends the current `getChainBlockCount()` (not hardcoded 1) so incremental sync works on reconnect in `src/network/PeerClient.cpp`

**Checkpoint**: A node that was offline catches up by downloading only missing blocks. Incremental sync verified.

---

## Phase 5: User Story 3 — Node Rejects Invalid Chain Data (Priority: P2)

**Goal**: A syncing node validates every block and rejects chunks with invalid data.

**Independent Test**: Send a BLOCKCHAIN_RESPONSE with a tampered block hash — the node rejects the chunk and logs an error.

### Tests for User Story 3

- [ ] T023 [P] [US3] Unit test: PeerClient rejects chunk containing block with invalid hash in `tests/sync_tests.cpp`
- [ ] T024 [P] [US3] Unit test: PeerClient rejects chunk containing block with insufficient PoW difficulty in `tests/sync_tests.cpp`
- [ ] T025 [P] [US3] Unit test: PeerClient accepts valid longer chain and replaces local chain in `tests/sync_tests.cpp`
- [ ] T026 [P] [US3] Unit test: PeerClient keeps local chain when peer chain is same length (longest-chain rule) in `tests/sync_tests.cpp`
- [ ] T027 [P] [US3] Unit test: PeerClient skips sync when peer's `total_chain_height` is less than local height in `tests/sync_tests.cpp`

### Implementation for User Story 3

- [ ] T028 [US3] Add per-chunk rejection logic — on validation failure, discard the invalid chunk, abort sync, return to IDLE, log error in `src/network/PeerClient.cpp`
- [ ] T029 [US3] Add longest-chain guard — before starting sync, compare peer's `total_chain_height` with local height; skip sync if peer is not strictly longer in `src/network/PeerClient.cpp`
- [ ] T030 [US3] Add validation error logging — log which block index failed, the validation reason, and the peer identity in `src/network/PeerClient.cpp`

**Checkpoint**: Invalid chain data is rejected per-chunk. Valid longer chains accepted. Equal-length chains not replaced.

---

## Phase 6: User Story 4 — Sync Handles Network Interruptions Gracefully (Priority: P3)

**Goal**: Sync survives connection drops — already-persisted chunks preserved, sync resumes on reconnect.

**Independent Test**: Initiate sync, sever connection mid-transfer, reconnect — sync completes without data loss.

### Tests for User Story 4

- [ ] T031 [P] [US4] Unit test: PeerClient preserves already-persisted chunks when connection drops mid-sync in `tests/sync_tests.cpp`
- [ ] T032 [P] [US4] Unit test: 60-second per-chunk timeout fires and aborts sync in `tests/sync_tests.cpp`
- [ ] T033 [P] [US4] Unit test: PeerClient returns to IDLE state on connection error during sync in `tests/sync_tests.cpp`

### Implementation for User Story 4

- [ ] T034 [US4] Add 60-second per-chunk `boost::asio::steady_timer` deadline in PeerClient response handler — abort sync on timeout in `src/network/PeerClient.cpp`
- [ ] T035 [US4] Add connection error handler during sync — on `boost::system::error_code`, preserve local state, transition to IDLE, log error in `src/network/PeerClient.cpp`
- [ ] T036 [US4] Ensure auto-sync on reconnect uses updated local chain height so sync resumes from last persisted chunk in `src/network/PeerClient.cpp`

**Checkpoint**: Sync is resilient to network interruptions. Timeouts and disconnects handled gracefully.

---

## Phase 7: RPC Integration (Cross-Story)

**Purpose**: `requestSync` RPC method and `addBlock` blocking — depends on sync logic from US1

- [ ] T037 [P] Unit test: `requestSync` RPC returns `sync_started` when not syncing in `tests/server_tests.cpp`
- [ ] T038 [P] Unit test: `requestSync` RPC returns error `-32002` when sync already in progress in `tests/server_tests.cpp`
- [ ] T039 [P] Unit test: `addBlock` RPC returns error `-32001` when sync is active in `tests/server_tests.cpp`
- [ ] T040 [P] Unit test: `addBlock` RPC succeeds normally when sync is not active in `tests/server_tests.cpp`
- [ ] T041 [P] Unit test: read-only RPCs (`getBlockByIndex`, `getBlocksByKeys`) succeed while `isSyncing` is true in `tests/server_tests.cpp`
- [ ] T042 Implement `requestSync` RPC method in `RpcServer` — check sync state, trigger `PeerClient` sync, return result in `src/network/RpcServer.cpp`
- [ ] T043 Gate `addBlock` on `SyncState` — check `isSyncing` flag at start of addBlock handler; return error `-32001` if true in `src/network/RpcServer.cpp`
- [ ] T044 Add JSON-RPC helper methods: `syncInProgressMessage()`, `syncStartedMessage()`, `noPeerMessage()` in `src/network/RpcServer.hpp` and `src/network/RpcServer.cpp`

**Checkpoint**: `requestSync` RPC works. `addBlock` blocked during sync. Read-only RPCs unaffected.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Final validation, build integration, cleanup

- [ ] T045 Verify `make check` passes all sync and RPC tests
- [ ] T046 Run quickstart.md scenario 1 (initial sync) end-to-end manually and verify
- [ ] T047 Run quickstart.md scenario 3 (addBlock blocked during sync) and verify error response
- [ ] T048 Verify concurrent sync request handling — PeerServer handles multiple BLOCKCHAIN_QUERY packets from different connections without data corruption

**Checkpoint**: All tests pass, quickstart scenarios verified, feature complete.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — can start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 (T001, T002) — BLOCKS all user stories
- **Phase 3 (US1)**: Depends on Phase 2 — core sync pipeline
- **Phase 4 (US2)**: Depends on Phase 3 (T012–T015) — extends sync with incremental logic
- **Phase 5 (US3)**: Depends on Phase 3 (T014) — extends validation path
- **Phase 6 (US4)**: Depends on Phase 3 (T013) — adds timeout and error recovery
- **Phase 7 (RPC)**: Depends on Phase 1 (T003) and Phase 3 (T012) — RPC needs sync state flag and PeerClient sync trigger
- **Phase 8 (Polish)**: Depends on all previous phases

### User Story Dependencies

- **US1 (P1)**: Depends on Foundational (Phase 2) only — MVP slice
- **US2 (P2)**: Depends on US1 core implementation (T012–T015)
- **US3 (P2)**: Depends on US1 validation path (T014). Can run in parallel with US2.
- **US4 (P3)**: Depends on US1 response handler (T013). Can run in parallel with US2 and US3.
- **RPC**: Depends on SyncState (T003) and US1 (T012). Can run in parallel with US2–US4.

### Parallel Opportunities

- **Phase 1**: T001+T002 same file but sequential; T003 parallel (different file); T004 parallel; T005 parallel
- **Phase 3**: All US1 tests (T009, T010, T011) can be written in parallel before implementation
- **Phase 5 + Phase 6**: US3 and US4 can proceed in parallel after US1 completes
- **Phase 7**: RPC tasks (T037–T041 tests) can all be written in parallel

---

## Parallel Example: After Phase 3 (US1) Completes

```
Developer A: Phase 4 (US2 — incremental sync)
Developer B: Phase 5 (US3 — validation rejection)
Developer C: Phase 6 (US4 — timeout/error handling)
Developer D: Phase 7 (RPC integration)
```

All four can proceed simultaneously since they extend different aspects of the US1 core.

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T005)
2. Complete Phase 2: Foundational (T006–T008)
3. Complete Phase 3: User Story 1 (T009–T017)
4. **STOP and VALIDATE**: Fresh node syncs full chain from peer
5. This is a functional, end-to-end chain sync

### Incremental Delivery

1. Setup + Foundational → Protocol structures and server handler ready
2. Add US1 → Full initial sync works → **MVP!**
3. Add US2 → Incremental catch-up sync works
4. Add US3 → Invalid data rejected, security hardened
5. Add US4 → Network interruptions handled gracefully
6. Add RPC → Operator tools (requestSync, addBlock gating)
7. Polish → Quickstart validated, make check green

---

## Notes

- All tasks use existing approved dependencies only (Boost, OpenSSL, nlohmann/json, Catch2)
- Tests use `MockChunk` for Blockchain instantiation in unit tests
- No new directories — all code in existing `src/`, `src/network/`, `tests/`
- `SyncMessages.hpp` and `SyncState.hpp` are the only new header files
