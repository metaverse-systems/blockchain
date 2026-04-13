# Tasks: Performance & Deduplication Cleanup

**Input**: Design documents from `/specs/019-perf-dedup-cleanup/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Not explicitly requested in the feature specification. Test tasks are omitted. Existing tests are refactored (not new tests).

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: No project initialization needed — this feature modifies an existing codebase. Phase 1 is a no-op.

_(No tasks)_

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared utilities that multiple user stories depend on. Must complete before story work begins.

- [ ] T001 Add `LOG_DEBUG`, `LOG_INFO`, `LOG_WARN`, `LOG_ERROR` macros to `src/utils.hpp` that check `getLogLevel()` before evaluating the message expression
- [ ] T002 [P] Create `src/network/PacketSerializer.hpp` with header-only `serialize_packet<T>()` template that returns `std::pair<std::vector<char>, std::string>` (header bytes + serialized payload)
- [ ] T003 [P] Add `make_block(size_t index, const std::string &prevHash)` helper to `tests/TestHelpers.hpp` for fast difficulty-0 block creation matching chunk_persistence_tests usage

**Checkpoint**: Foundation ready — shared utilities in place for all user stories.

---

## Phase 3: User Story 1 — Faster Peer Operations Under Load (Priority: P1) 🎯 MVP

**Goal**: Replace O(n) linear scans in PeerManager with O(1) hash map lookups for peers and bans.

**Independent Test**: Build and run `./tests/blockchain_tests` (contains peer_manager_tests, peer_discovery_tests) — all peer operations exercise the new data structure.

### Implementation for User Story 1

- [ ] T004 [US1] Change `peers_` from `std::vector<PeerEntry>` to `std::unordered_map<std::string, PeerEntry>` in `src/PeerManager.hpp` and update includes
- [ ] T005 [US1] Change `bans_` from `std::vector<BanRecord>` to `std::unordered_map<std::string, BanRecord>` in `src/PeerManager.hpp`
- [ ] T006 [US1] Rewrite `find_peer()` (both const and non-const overloads) in `src/PeerManager.cpp` to use `peers_.find(peer_key(host, port))`
- [ ] T007 [US1] Rewrite `add_peer()` in `src/PeerManager.cpp` to use map insert-or-update via `peers_[key]`; preserve `normalize_address()` call and cap enforcement with `evict_oldest_peer()`
- [ ] T008 [US1] Rewrite `remove_peer()` in `src/PeerManager.cpp` to use `peers_.erase(peer_key(host, port))`
- [ ] T009 [US1] Rewrite `is_banned()` in `src/PeerManager.cpp` to use `bans_.find(peer_key(host, port))` with expiry check
- [ ] T010 [US1] Update `ban_peer()` and `unban_peer()` in `src/PeerManager.cpp` to use map insert/erase on `bans_`
- [ ] T011 [US1] Update `purge_expired_bans()` in `src/PeerManager.cpp` to iterate and erase expired entries from `bans_` map
- [ ] T012 [US1] Update `get_peers()` in `src/PeerManager.cpp` to collect map values into a `std::vector<PeerEntry>`
- [ ] T013 [US1] Update `get_bans()` in `src/PeerManager.cpp` to collect map values into a `std::vector<BanRecord>`
- [ ] T014 [US1] Update `get_non_banned_peer_addresses()` in `src/PeerManager.cpp` to iterate map values
- [ ] T015 [US1] Update `evict_oldest_peer()` in `src/PeerManager.cpp` to find min `last_seen` across map values and erase by key
- [ ] T016 [US1] Update `save_peers()` in `src/PeerManager.cpp` to serialize map values as a JSON array (preserving `peers.json` format)
- [ ] T017 [US1] Update `load_peers()` in `src/PeerManager.cpp` to deserialize JSON array into map entries using `peer_key()` as key
- [ ] T018 [US1] Update remaining iteration sites in `src/PeerManager.cpp`: `start()`, `on_peer_exchange_received()`, `send_to_peers()`, `increment_error()` to use map iteration
- [ ] T019 [US1] Build with `make -j8` and run `./tests/blockchain_tests` — verify all peer_manager_tests and peer_discovery_tests pass
- [ ] T020 [US1] Run `./tests/p2p_sync_integration_tests` to verify P2P integration with new peer data structure

**Checkpoint**: Peer operations are O(1). All peer-related tests pass. MVP delivered.

---

## Phase 4: User Story 2 — Efficient RPC Request Routing (Priority: P2)

**Goal**: Replace the 20-branch if/else chain in `RpcServer::do_read()` with an `std::unordered_map` dispatch table of individually testable handler functions.

**Independent Test**: Build and run `./tests/rpc_integration_tests` — exercises all 20 RPC methods over live SSL sockets.

### Implementation for User Story 2

- [ ] T021 [US2] Add `RpcHandler` type alias and `dispatch_` member to `src/network/RpcServer.hpp`: `using RpcHandler = std::function<nlohmann::json(const nlohmann::json &)>;` and `std::unordered_map<std::string, RpcHandler> dispatch_;`
- [ ] T022 [US2] Extract `handle_publish()` as a private method in `src/network/RpcServer.hpp` / `src/network/RpcServer.cpp` — move the `publish` handler body from `do_read()` into this method, returning JSON response
- [ ] T023 [US2] Extract `handle_createStream()` and `handle_listStreams()` as private methods in `src/network/RpcServer.cpp`
- [ ] T024 [P] [US2] Extract `handle_getStreamEntries()` and `handle_getStreamEntry()` as private methods in `src/network/RpcServer.cpp`
- [ ] T025 [P] [US2] Extract `handle_requestSync()` as a private method in `src/network/RpcServer.cpp`
- [ ] T026 [P] [US2] Extract `handle_getBlockByIndex()`, `handle_getBlocksByKeys()`, `handle_getBlockRange()`, `handle_getBlockHeader()` as private methods in `src/network/RpcServer.cpp`
- [ ] T027 [P] [US2] Extract `handle_addPeer()`, `handle_removePeer()`, `handle_listPeers()` as private methods in `src/network/RpcServer.cpp`
- [ ] T028 [P] [US2] Extract `handle_banPeer()`, `handle_unbanPeer()` as private methods in `src/network/RpcServer.cpp`
- [ ] T029 [P] [US2] Extract `handle_getInclusionProof()`, `handle_verifyInclusionProof()` as private methods in `src/network/RpcServer.cpp`
- [ ] T030 [P] [US2] Extract `handle_getNodeStatus()`, `handle_getChainLength()`, `handle_getChunkCount()` as private methods in `src/network/RpcServer.cpp`
- [ ] T031 [US2] Initialize `dispatch_` table in `RpcServer` constructor in `src/network/RpcServer.cpp` — register all 20 handlers by method name string
- [ ] T032 [US2] Replace the if/else chain in `do_read()` in `src/network/RpcServer.cpp` with dispatch table lookup: `auto it = dispatch_.find(method); if (it != dispatch_.end()) response = it->second(object); else response = invalidMethodMessage(...)` — wire `outputStream << response` and `do_write()` after dispatch
- [ ] T033 [US2] Build with `make -j8` and run `./tests/rpc_integration_tests` — verify all 20 RPC methods return identical responses
- [ ] T034 [US2] Run `./tests/rpc_expansion_tests` to verify RPC error code handling is preserved

**Checkpoint**: RPC dispatch is O(1). Handlers are individually testable. All RPC tests pass.

---

## Phase 5: User Story 3 — Consolidated Packet Serialization (Priority: P2)

**Goal**: Replace duplicated send templates in PeerClient and PeerServer with the shared `serialize_packet<T>()` utility from Phase 2.

**Independent Test**: Build and run `./tests/p2p_sync_integration_tests` and `./tests/block_propagation_integration_tests` — exercises P2P packet send/receive paths.

### Implementation for User Story 3

- [ ] T035 [US3] Replace body of `PeerClient::send<T>()` in `src/network/PeerClient.cpp` with call to `serialize_packet<T>()` from `PacketSerializer.hpp`, keeping the existing `async_write` with `write_buffer` and callback
- [ ] T036 [US3] Replace body of `PeerServer::send_packet<T>()` in `src/network/PeerServer.cpp` with call to `serialize_packet<T>()` from `PacketSerializer.hpp`, keeping the existing `async_write` with shared_ptr buffers and callback
- [ ] T037 [US3] Build with `make -j8` and run `./tests/p2p_sync_integration_tests` and `./tests/block_propagation_integration_tests` — verify wire-format compatibility
- [ ] T037a [US3] Add a Catch2 unit test in `tests/blockchain_tests` that calls `serialize_packet<Block>()` and verifies the returned header has the correct packet type and payload length matching a manual Boost.Serialization of the same object

**Checkpoint**: Packet serialization lives in exactly one place. P2P integration tests pass.

---

## Phase 6: User Story 4 — Consolidated Test Helpers (Priority: P3)

**Goal**: Remove all local test helper definitions from the four test files and use the shared `TestHelpers.hpp` instead.

**Independent Test**: Build and run the full test suite — all test binaries produce identical pass/fail results.

### Implementation for User Story 4

- [ ] T038 [P] [US4] In `tests/sync_tests.cpp`: remove local `mineTestBlock()` and `buildValidChain()` definitions, add `#include "TestHelpers.hpp"`, replace calls with `TestHelpers::mineTestBlock(...)` and `TestHelpers::buildValidChain(...)`
- [ ] T039 [P] [US4] In `tests/consensus_tests.cpp`: remove local `mineBlock()` definition, add `#include "TestHelpers.hpp"`, replace calls with `TestHelpers::mineTestBlock(...)`
- [ ] T040 [P] [US4] In `tests/block_propagation_tests.cpp`: remove local helper definitions, add `#include "TestHelpers.hpp"`, replace calls with `TestHelpers::` equivalents
- [ ] T041 [P] [US4] In `tests/chunk_persistence_tests.cpp`: remove local `make_block()` definition, add `#include "TestHelpers.hpp"`, replace calls with `TestHelpers::make_block(...)`
- [ ] T042 [US4] Build with `make -j8` and run all test binaries individually — verify identical pass/fail results

**Checkpoint**: Zero local test helper definitions outside `TestHelpers.hpp`. Full suite passes.

---

## Phase 7: User Story 5 — Reduced Unnecessary Log Allocations (Priority: P3)

**Goal**: Migrate hot-path `logMessage()` calls to the lazy `LOG_*` macros so suppressed messages produce zero heap allocations.

**Independent Test**: Code inspection — verify hot-path calls use macros; build and run `./tests/blockchain_tests` to confirm no regressions.

### Implementation for User Story 5

- [ ] T043 [US5] Convert `logMessage()` calls in `src/PeerManager.cpp` hot paths (peer exchange, block relay, connection attempts) to `LOG_INFO(...)` / `LOG_DEBUG(...)` / `LOG_WARN(...)` / `LOG_ERROR(...)` macros
- [ ] T044 [P] [US5] Convert `logMessage()` calls in `src/BlockPropagation.cpp` to `LOG_*` macros
- [ ] T045 [P] [US5] Convert `logMessage()` calls in `src/network/PeerClient.cpp` and `src/network/PeerServer.cpp` to `LOG_*` macros
- [ ] T046 [US5] Build with `make -j8` and run `./tests/blockchain_tests`, `./tests/p2p_sync_integration_tests`, `./tests/block_propagation_integration_tests` — verify no regressions

**Checkpoint**: Hot-path log calls use lazy macros. Suppressed messages produce zero allocations.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Documentation updates, audit tracking, and validation.

- [X] T047 [P] Update `docs/AUDIT.md` — mark §4.1 (O(n) peer lookups), §4.2 (RPC dispatch), §4.4 (log allocations), §5.1 (packet serialization), and §5.2 (test helpers) as ✅ RESOLVED with references to this feature (019)
- [X] T048 [P] Update `docs/ROADMAP.md` — move 019-perf-dedup-cleanup from "Suggested Specs" or "In Progress" to "Completed" with one-line summary
- [X] T049 Run quickstart.md verification checklist: confirm all 5 items pass
- [X] T050 Final full build (`make -j8`) and run all test binaries individually to confirm zero regressions

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No-op
- **Foundational (Phase 2)**: No dependencies — can start immediately
  - T001 (log macros), T002 (PacketSerializer), T003 (TestHelpers make_block) are independent [P]
- **US1 — Peer Operations (Phase 3)**: No dependency on Phase 2 utilities (doesn't use PacketSerializer or make_block)
  - **Can start in parallel with Phase 2**
- **US2 — RPC Dispatch (Phase 4)**: No dependency on Phase 2 or Phase 3
  - **Can start in parallel with Phases 2 and 3**
- **US3 — Packet Serialization (Phase 5)**: Depends on T002 (PacketSerializer.hpp from Phase 2)
- **US4 — Test Helpers (Phase 6)**: Depends on T003 (make_block from Phase 2), and should run after US1 (Phase 3) since peer_manager_tests may be affected
- **US5 — Log Macros (Phase 7)**: Depends on T001 (macros from Phase 2), and should run after US1 (Phase 3) since PeerManager calls are migrated
- **Polish (Phase 8)**: Depends on all user stories being complete

### User Story Dependencies

- **US1 (P1)**: Independent — can start immediately
- **US2 (P2)**: Independent — can start immediately
- **US3 (P2)**: Depends on T002 only
- **US4 (P3)**: Depends on T003; best after US1 completes
- **US5 (P3)**: Depends on T001; best after US1 and US3 complete (modified files settle)

### Within Each User Story

- Modify header declaration before implementation (.hpp before .cpp changes)
- Core operations before auxiliary operations
- Functional changes before verification (build + test at end of each phase)

### Parallel Opportunities

- **Phase 2**: All three foundational tasks (T001, T002, T003) can run in parallel
- **Phase 3 + Phase 4**: US1 and US2 are fully independent — can run in parallel
- **Phase 4 handlers**: T024–T030 extract different handler groups — all can run in parallel
- **Phase 6**: T038–T041 modify different test files — all can run in parallel
- **Phase 7**: T043–T045 modify different source files — T044 and T045 can run in parallel
- **Phase 8**: T047 and T048 modify different doc files — can run in parallel

---

## Parallel Example: Phases 2 + 3 + 4

```
# These three workstreams can proceed simultaneously:

# Workstream A (Phase 2): Foundational utilities
T001: Add LOG_* macros to src/utils.hpp
T002: Create src/network/PacketSerializer.hpp
T003: Add make_block() to tests/TestHelpers.hpp

# Workstream B (Phase 3/US1): Peer operations
T004–T020: PeerManager vector→map migration

# Workstream C (Phase 4/US2): RPC dispatch
T021–T034: RpcServer handler extraction + dispatch table
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 2: Foundational (T001–T003)
2. Complete Phase 3: US1 — Peer Operations (T004–T020)
3. **STOP and VALIDATE**: Run `./tests/blockchain_tests` and `./tests/p2p_sync_integration_tests`
4. The highest-impact performance improvement is delivered

### Incremental Delivery

1. Phase 2 (Foundational) → shared utilities ready
2. Phase 3 (US1: Peer Ops) → O(1) peer lookups (**MVP**)
3. Phase 4 (US2: RPC Dispatch) → O(1) RPC dispatch + maintainability
4. Phase 5 (US3: Packet Serialize) → deduplication resolved
5. Phase 6 (US4: Test Helpers) → test code consolidated
6. Phase 7 (US5: Log Macros) → lazy formatting on hot paths
7. Phase 8 (Polish) → docs updated, full validation

### Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- Constitution requires: `make -j8`, individual test binary execution, roadmap update
