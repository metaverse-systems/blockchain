# Tasks: Integration Test Suite

**Input**: Design documents from `/specs/012-integration-test-suite/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization — build system registration for new test binaries

- [X] T001 Register `rpc_integration_tests` and `p2p_sync_integration_tests` binaries in tests/Makefile.am following the existing test binary pattern (sources, flags, link libs)
- [X] T002 Add `rpc_integration_tests` and `p2p_sync_integration_tests` to .gitignore

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared test infrastructure that MUST be complete before ANY user story test can be written

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T003 Implement TLS certificate generation utility in tests/IntegrationTestFixture.hpp — `generate_self_signed_cert(temp_dir)` function using OpenSSL EVP/X509 API (RSA-2048, V3, CN=localhost, SHA-256 self-signed, PEM output) per research.md R1
- [X] T004 Implement NodeInstance class in tests/IntegrationTestFixture.hpp — manages a single in-process blockchain node: temp data directory creation, cert generation, dynamic port-0 binding for RPC and P2P acceptors, SSL context setup (RPC server-only TLS + P2P mutual TLS with self-signed cert as CA), Blockchain\<Chunk\> construction with difficulty=0, Server\<RpcServer\> and Server\<PeerServer\> wiring, `io_context.run()` on background thread, startup readiness check with 10-second timeout (verify acceptor is listening before returning), RAII stop/cleanup per research.md R3 and data-model.md
- [X] T005 Implement RpcTestClient class in tests/IntegrationTestFixture.hpp — synchronous TLS client: connect to host:port with certificate verification disabled, `call(method, params)` sends newline-terminated JSON-RPC 2.0 request and reads newline-terminated response with a 10-second read deadline via `steady_timer` + `async_read_until`, returns parsed nlohmann::json per research.md R4 and contracts/json-rpc-test-contract.md
- [X] T006 Implement IntegrationTestFixture class in tests/IntegrationTestFixture.hpp — manages base temp directory, `create_node()` factory returning NodeInstance pointer with unique subdir, destructor stops all nodes then removes base temp dir per data-model.md

**Checkpoint**: Foundation ready — both test binaries can create nodes and RPC clients

---

## Phase 3: User Story 1 — Verify RPC Endpoints Over Real TLS (Priority: P1) 🎯 MVP

**Goal**: All JSON-RPC methods work end-to-end over actual TLS connections with correct responses and proper error handling

**Independent Test**: Start a single node with self-signed cert, connect RPC client, exercise every registered method

### Implementation for User Story 1

- [X] T007 [US1] Create rpc_integration_tests.cpp scaffold with `#define CATCH_CONFIG_MAIN`, includes for IntegrationTestFixture.hpp and Catch2, and a shared fixture that starts one NodeInstance with `allowed_streams = {"teststream"}`; add a Catch2 listener or per-test watchdog that fails any test exceeding 60 seconds (SC-006) in tests/rpc_integration_tests.cpp
- [X] T008 [P] [US1] Implement `addBlock` positive test — call with valid data string, verify result is a block object with `index` > 0 and `getChainLength` increases in tests/rpc_integration_tests.cpp
- [X] T009 [P] [US1] Implement `getChainLength` positive test — call method on fresh node (genesis only), verify result == 1 in tests/rpc_integration_tests.cpp
- [X] T010 [P] [US1] Implement `getChunkCount` positive test — verify result == 1 on fresh node in tests/rpc_integration_tests.cpp
- [X] T011 [P] [US1] Implement `getNodeStatus` positive test — verify response contains all 7 fields (`chainLength`, `chunkCount`, `syncState`, `currentDifficulty`, `inboundPeers`, `outboundPeers`, `nodeUuid`), `syncState` == "idle" in tests/rpc_integration_tests.cpp
- [X] T012 [US1] Implement `publish` positive test — publish entry to "teststream" with key and data, verify result is a block object with `index` > 0, then call `getChainLength` and verify it increased in tests/rpc_integration_tests.cpp
- [X] T013 [US1] Implement `getStreamEntries` positive test — after publish (T012 pattern), call with stream name and verify returned entries contain published data in tests/rpc_integration_tests.cpp
- [X] T014 [US1] Implement `getBlockRange` positive test — after publishing entries, call with `start=0, end=1`, verify array of 2 block objects returned in tests/rpc_integration_tests.cpp
- [X] T015 [US1] Implement `getBlockRange` negative test — call with `start=5, end=3` (invalid range), verify error code -32602 in tests/rpc_integration_tests.cpp
- [X] T016 [US1] Implement `getBlockHeader` positive test — call with `blockIndex=0` (genesis), verify response has all 7 header fields (`index`, `timestamp`, `prevHash`, `merkleRoot`, `nonce`, `difficulty`, `hash`) in tests/rpc_integration_tests.cpp
- [X] T017 [US1] Implement `getBlockHeader` negative test — call with out-of-range index, verify error code -32001 in tests/rpc_integration_tests.cpp
- [X] T018 [US1] Implement `getInclusionProof` positive test — after publishing an entry, call with valid blockIndex and entryIndex=0, verify response has `blockIndex`, `entryIndex`, `merkleRoot`, `leafHash`, `proof` fields in tests/rpc_integration_tests.cpp
- [X] T019 [US1] Implement `getInclusionProof` negative test — call with out-of-range blockIndex, verify error code -32001 in tests/rpc_integration_tests.cpp
- [X] T020 [US1] Implement `verifyInclusionProof` positive test — get proof via `getInclusionProof` then verify it; assert `valid == true` and `merkleRoot` matches in tests/rpc_integration_tests.cpp
- [X] T021 [US1] Implement `verifyInclusionProof` tamper test — submit proof with tampered leafHash (64 'f' chars), assert `valid == false` in tests/rpc_integration_tests.cpp
- [X] T022 [US1] Implement `publish` negative test — publish to a stream NOT in `allowed_streams`, verify error code -32003 in tests/rpc_integration_tests.cpp
- [X] T023 [US1] Implement `requestSync` negative test — call on node with no peer client configured, verify error code -32603 in tests/rpc_integration_tests.cpp
- [X] T024 [US1] Implement unknown method test — call `{"method": "nonExistentMethod"}`, verify error code -32601 in tests/rpc_integration_tests.cpp
- [X] T025 [US1] Implement malformed JSON test — send `not valid json\n`, verify error code -32600 in tests/rpc_integration_tests.cpp
- [X] T026 [US1] Implement missing `id` field test — send valid JSON-RPC without `id`, verify error response in tests/rpc_integration_tests.cpp

**Checkpoint**: All 11 RPC methods tested (including `addBlock`) with positive + negative cases over real TLS. US1 is independently functional.

---

## Phase 4: User Story 2 — Two-Node P2P Sync Over Real TLS (Priority: P2)

**Goal**: Two blockchain nodes connect over TLS, synchronize chains, and propagate new blocks

**Independent Test**: Start two nodes with separate data dirs, configure one as seed peer of the other, verify sync completes and blocks propagate

### Implementation for User Story 2

- [X] T027 [US2] Create p2p_sync_integration_tests.cpp scaffold with `#define CATCH_CONFIG_MAIN`, includes for IntegrationTestFixture.hpp and Catch2; add a per-test watchdog that fails any test exceeding 60 seconds (SC-006) in tests/p2p_sync_integration_tests.cpp
- [X] T028 [US2] Implement full-chain sync test — start Node A, publish 10 blocks (difficulty=0 for fast mining), start Node B configured with Node A as seed peer, wait (poll with timeout) until Node B's `getChainLength` matches Node A's, assert within 30 seconds in tests/p2p_sync_integration_tests.cpp
- [X] T029 [US2] Implement block propagation test — start two nodes, wait for sync to complete, publish a new block on Node A via RPC, poll Node B until its chain length increases by 1, assert propagation within 10 seconds in tests/p2p_sync_integration_tests.cpp
- [X] T030 [US2] Implement peer discovery test — start Node A and Node B (connected to A), start Node C connected to B only, verify Node C discovers Node A via peer exchange and synchronizes full chain in tests/p2p_sync_integration_tests.cpp

**Checkpoint**: P2P sync, propagation, and peer discovery validated over real TLS. US2 is independently functional.

---

## Phase 5: User Story 3 — Build System Integration (Priority: P3)

**Goal**: Integration tests compile and run via standard build commands

**Independent Test**: Run `make -j8` and execute each binary individually

### Implementation for User Story 3

- [X] T031 [US3] Verify `make -j8` compiles both new test binaries without errors — run build, fix any compilation issues
- [X] T032 [US3] Run `./tests/rpc_integration_tests` and verify all test cases pass with exit code 0
- [X] T033 [US3] Run `./tests/p2p_sync_integration_tests` and verify all test cases pass with exit code 0

**Checkpoint**: Both test binaries compile and pass. US3 is complete.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Cleanup, validation, and roadmap update

- [X] T034 Verify no temporary files or directories remain after test runs (SC-004)
- [X] T035 Verify tests can run in parallel with other test binaries without port conflicts (SC-005)
- [X] T036 Run quickstart.md validation checklist
- [X] T037 Update docs/ROADMAP.md — move 012 Integration Test Suite to Completed table with summary

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Phase 2 — can start after fixture is complete
- **US2 (Phase 4)**: Depends on Phase 2 — can start in parallel with US1 (different files)
- **US3 (Phase 5)**: Depends on US1 + US2 test code being written
- **Polish (Phase 6)**: Depends on all user stories passing

### Within User Story 1

- T007 (scaffold) must complete before T008–T025
- T008, T009, T010 can run in parallel (independent read-only methods)
- T011 must complete before T012, T013, T017, T019 (they depend on published data pattern)
- T014, T016, T018, T020, T021, T022, T023, T024, T025 are independent of each other

### Within User Story 2

- T026 (scaffold) must complete before T027–T029
- T027 must complete before T028 (propagation test builds on sync pattern)
- T029 can start after T027 (uses same pattern with a third node)

### Parallel Opportunities

```
Phase 2 (sequential):
  T003 → T004 → T005 → T006

Phase 3 + Phase 4 (parallel across stories):
  T007 → T008,T009,T010 (parallel) → T011 → T012..T025
  T026 → T027 → T028,T029 (parallel)
```

---

## Implementation Strategy

### MVP Scope

**User Story 1 (RPC over TLS)** is the MVP. After Phase 2 + Phase 3, developers have confidence that all RPC endpoints work over real TLS connections. This is independently valuable and does not require multi-node infrastructure.

### Incremental Delivery

1. **Phase 1–2**: Build system + shared fixture (~T001–T006)
2. **Phase 3**: RPC tests — delivers US1 independently (~T007–T025)
3. **Phase 4**: P2P tests — delivers US2 independently (~T026–T029)
4. **Phase 5–6**: Validate, polish, update roadmap (~T030–T036)
