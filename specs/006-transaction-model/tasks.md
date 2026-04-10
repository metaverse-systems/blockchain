# Tasks: Transaction Model (Stream-Based Key/Value Store)

**Input**: Design documents from `/specs/006-transaction-model/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

**Tests**: Included — constitution §III mandates full test coverage. New test files: `stream_entry_tests.cpp`, `stream_tests.cpp`. Modified: `block_tests.cpp`, `server_tests.cpp`.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Core data structures — StreamEntry struct and Block struct migration

- [ ] T001 Create StreamEntry struct with Boost.Serialization and validation helpers in src/StreamEntry.hpp
- [ ] T002 Update Block struct in src/Block.hpp — remove `data` field, add `std::vector<StreamEntry> entries`, include StreamEntry.hpp, update serialize template and constructor declarations
- [ ] T003 Update Block implementation in src/Block.cpp — update constructors (replace data param with entries), update calculateHash to include serialized entries, update toJson to output entries array, update dump

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Interface, Blockchain core, and build system changes that ALL user stories depend on

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T004 Update IBlockchain interface in src/IBlockchain.hpp — replace `addBlock` virtual method with `publish(const std::string &stream, const std::string &key, const std::string &data, const std::vector<std::string> &keys)`, add virtual methods for `createStream`, `listStreams`, `getStreamEntries`, `getStreamEntry`, add stream entry validation (name regex, key non-empty, data size) to `isValidNewBlock`
- [ ] T005 Update Blockchain template in src/Blockchain.hpp — add `std::set<std::string>` stream registry, add `std::map<std::string, std::map<std::string, std::vector<size_t>>>` stream index, replace `addBlock` with `publish`, declare `createStream`, `listStreams`, `getStreamEntries`, `getStreamEntry`, declare `saveStreams`/`loadStreams` and `saveStreamIndex`/`loadStreamIndex`
- [ ] T006 Implement Blockchain stream methods in src/Blockchain.cpp — implement `publish` (validate entry, auto-create stream, mine block with entries, update stream index), implement `createStream` (duplicate check), `listStreams`, `getStreamEntries` (history mode), `getStreamEntry` (latest mode), implement stream/index persistence via Boost.Serialization to `streams.dat`/`stream_index.dat`, update `appendBlock` to maintain stream index and registry, update `generateGenesisBlock` for Block without data field
- [ ] T007 [P] Update tests/MockBlockchain.hpp — replace `addBlock` with `publish`, add `createStream`/`listStreams`/`getStreamEntries`/`getStreamEntry` stubs, update `createValidNextBlock` helper to use entries instead of data
- [ ] T008 [P] Update tests/Makefile.am — add `stream_entry_tests.cpp` and `stream_tests.cpp` to `blockchain_tests_SOURCES`

**Checkpoint**: Foundation ready — all new data structures, interfaces, and core methods in place. User story phases can now begin.

---

## Phase 3: User Story 1 — Publish Data to a Stream (Priority: P1) 🎯 MVP

**Goal**: Replace the `addBlock` RPC endpoint with `publish` so clients can submit data to named streams with keys.

**Independent Test**: Publish an entry via RPC and verify the resulting block contains the correctly serialized StreamEntry.

### Implementation for User Story 1

- [ ] T009 [US1] Replace addBlock RPC handler with publish handler in src/network/RpcServer.cpp and src/network/RpcServer.hpp — parse `stream`, `key`, `data`, `keys` params from JSON-RPC request, validate stream name format (regex `^[a-zA-Z0-9_-]{1,256}$`), validate key non-empty, validate data size ≤ 128 MB, call `blockchain.publish()`, return block JSON with entries array
- [ ] T010 [P] [US1] Create tests/stream_entry_tests.cpp — test StreamEntry Boost.Serialization round-trip, test stream name validation helper accepts valid names and rejects invalid (empty, too long, special chars), test data size limit check
- [ ] T011 [P] [US1] Update tests/block_tests.cpp — test Block construction with StreamEntry entries vector, test Block serialization round-trip preserves entries, test calculateHash changes when entries differ
- [ ] T012 [US1] Add publish RPC tests in tests/server_tests.cpp — test publish success returns block with entries, test missing stream param returns -32602, test missing key returns -32602, test invalid stream name returns -32602, test data exceeding 128 MB returns -32602, test publish during sync returns -32001

**Checkpoint**: At this point, clients can publish data to streams and blocks contain structured StreamEntry payloads. This is the MVP.

---

## Phase 4: User Story 2 — Query Data by Stream and Key (Priority: P2)

**Goal**: Enable clients to retrieve stream entries by stream name and key in both history and latest modes.

**Independent Test**: Publish entries to known streams/keys, then query via getStreamEntries and getStreamEntry and verify correct results.

### Implementation for User Story 2

- [ ] T013 [US2] Implement getStreamEntries RPC handler in src/network/RpcServer.cpp — parse `stream` (required) and `key` (optional) params, return JSON array of matching entries with block_index in chain order (history mode), return -32602 for missing stream param
- [ ] T014 [US2] Implement getStreamEntry RPC handler in src/network/RpcServer.cpp — parse `stream` and `key` (both required), return only the latest entry, return -32601 if no entries exist for stream+key
- [ ] T015 [US2] Update getBlockByIndex and getBlocksByKeys response JSON to include entries array in src/network/RpcServer.cpp
- [ ] T016 [P] [US2] Add stream query tests in tests/stream_tests.cpp — test getStreamEntries returns all entries for stream+key in chain order, test getStreamEntries returns all entries in a stream when key omitted, test getStreamEntry returns latest entry only, test getStreamEntry returns -32601 for nonexistent stream+key, test getBlockByIndex response includes entries

**Checkpoint**: Clients can publish AND query stream data. Both history and latest modes work.

---

## Phase 5: User Story 3 — Validate Stream Entries in Received Blocks (Priority: P2)

**Goal**: Ensure blocks received via P2P containing malformed stream entries are rejected before appending to the chain.

**Independent Test**: Construct a block with an invalid stream entry (empty name, oversized data) and send via P2P; verify the receiving node rejects it.

### Implementation for User Story 3

- [ ] T017 [US3] Verify P2P block acceptance invokes stream entry validation from `isValidNewBlock` (added in T004) in src/network/PeerServer.cpp — add entry-specific rejection logging with the failing field (stream name, key, or data size), ensure block is rejected when any entry fails validation
- [ ] T018 [P] [US3] Add P2P stream validation tests in tests/block_propagation_tests.cpp — test block with valid entries is accepted, test block with empty stream name is rejected, test block with empty key is rejected, test block with oversized data is rejected

**Checkpoint**: P2P layer rejects malformed stream entries, protecting chain integrity.

---

## Phase 6: User Story 4 — Create and List Streams (Priority: P3)

**Goal**: Allow explicit stream creation and listing of all known streams via RPC.

**Independent Test**: Create a stream explicitly, publish to another (auto-create), list all streams and verify both appear.

### Implementation for User Story 4

- [ ] T019 [US4] Implement createStream RPC handler in src/network/RpcServer.cpp — parse `name` param, validate stream name format, call `blockchain.createStream()`, return success message, return -32004 for duplicate stream, return -32602 for invalid name
- [ ] T020 [US4] Implement listStreams RPC handler in src/network/RpcServer.cpp — call `blockchain.listStreams()`, return JSON array of stream names sorted alphabetically
- [ ] T021 [P] [US4] Add stream management tests in tests/stream_tests.cpp — test explicit createStream success, test duplicate createStream returns -32004, test listStreams returns all streams, test auto-created stream via publish appears in listStreams

**Checkpoint**: Full stream lifecycle — create, auto-create, and list.

---

## Phase 7: User Story 5 — Configure Per-Node Stream Permissions (Priority: P3)

**Goal**: Allow node operators to restrict which streams accept local RPC publishes via config.json.

**Independent Test**: Configure allowed_streams in config.json, publish to an allowed stream (succeeds) and a blocked stream (rejected with -32003).

### Implementation for User Story 5

- [ ] T022 [US5] Add StreamsConfig struct to src/NodeConfig.hpp — define `struct StreamsConfig { std::vector<std::string> allowed_streams; }` member, add `StreamsConfig streams` field to NodeConfig
- [ ] T023 [US5] Parse streams config section in src/NodeConfig.cpp — read `streams.allowed_streams` array from config.json, update `default_json()` to include empty streams section, update `validate()` if needed
- [ ] T024 [US5] Add stream permission check in publish RPC handler in src/network/RpcServer.cpp — if `allowed_streams` is non-empty and stream not in list, return -32003 error; requires passing NodeConfig (or streams config) to RpcServer
- [ ] T025 [P] [US5] Add per-node permission tests in tests/node_config_tests.cpp — test parsing allowed_streams from config.json, test empty allowed_streams means all permitted; add permission-check tests in tests/stream_tests.cpp — test publish to allowed stream succeeds, test publish to blocked stream returns -32003

**Checkpoint**: Node operators can restrict publishing by stream name. P2P blocks are unaffected by local permissions.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Ensure all changes integrate cleanly and propagation works end-to-end

- [ ] T026 [P] Update src/BlockPropagation.cpp if any code references `Block::data` directly — ensure block propagation works with entries field
- [ ] T027 [P] Update any remaining references to `addBlock` across src/ and tests/ — grep for leftover `addBlock` calls and update to use `publish`
- [ ] T028 Run full test suite via `make check` and fix any compilation or test failures
- [ ] T029 Run quickstart.md validation — execute each example from specs/006-transaction-model/quickstart.md against a running node

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational (Phase 2)
- **User Story 2 (Phase 4)**: Depends on Foundational (Phase 2) — can run in parallel with US1
- **User Story 3 (Phase 5)**: Depends on Foundational (Phase 2) — can run in parallel with US1/US2
- **User Story 4 (Phase 6)**: Depends on Foundational (Phase 2) — can run in parallel with US1/US2/US3
- **User Story 5 (Phase 7)**: Depends on US1 (Phase 3) — needs publish handler to add permission check
- **Polish (Phase 8)**: Depends on all user stories being complete

### Within Each Phase

- Setup: T001 → T002 → T003 (sequential — each depends on the previous)
- Foundational: T004 → T005 → T006 (sequential); T007 and T008 parallel with T006
- US1: T009 first (handler), then T012; T010 and T011 parallel with T009
- US2: T013 → T014 → T015 (same file, sequential); T016 parallel
- US3: T017 implementation; T018 parallel
- US4: T019 → T020 (same file); T021 parallel
- US5: T022 → T023 (same class); T024 after T023; T025 parallel with T024
- Polish: T026 and T027 parallel; T028 after both; T029 after T028

---

## Parallel Example: User Story 1 (MVP)

```bash
# After Phase 2 (Foundational) is complete:

# Parallel batch 1 — different files:
Task T009: Replace addBlock with publish handler in src/network/RpcServer.cpp
Task T010: Create tests/stream_entry_tests.cpp (StreamEntry unit tests)
Task T011: Update tests/block_tests.cpp (Block with entries tests)

# Sequential — depends on T009:
Task T012: Add publish RPC tests in tests/server_tests.cpp
```

## Parallel Example: User Stories 2–4

```bash
# After Phase 2 (Foundational) is complete, US2/US3/US4 can start in parallel:

# Developer A (US2):
Task T013 → T014 → T015 (RpcServer.cpp handlers)
Task T016 (stream_tests.cpp — parallel with T013-T015)

# Developer B (US3):
Task T017 (PeerServer.cpp validation)
Task T018 (block_propagation_tests.cpp — parallel with T017)

# Developer C (US4):
Task T019 → T020 (RpcServer.cpp handlers)
Task T021 (stream_tests.cpp — parallel with T019-T020)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T003)
2. Complete Phase 2: Foundational (T004–T008)
3. Complete Phase 3: User Story 1 (T009–T012)
4. **STOP and VALIDATE**: Run `make check`, test publish via RPC
5. This delivers: structured stream entries in blocks, publish RPC, stream auto-creation

### Incremental Delivery

1. Setup + Foundational → Core data structures and Blockchain methods ready
2. Add US1 (Publish) → Test independently → **MVP!**
3. Add US2 (Query) → Test independently → Clients can read data back
4. Add US3 (P2P Validation) → Test independently → Network integrity secured
5. Add US4 (Create/List) → Test independently → Full stream management
6. Add US5 (Permissions) → Test independently → Node-level access control
7. Polish → Full test suite green, quickstart validated

### Suggested MVP Scope

User Story 1 (Publish) alone delivers a functional stream-based blockchain. Query, validation, management, and permissions are valuable but not required for basic operation.

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks in that phase
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable after Phase 2
- Commit after each task or logical group
- Stop at any checkpoint to validate the story independently
- Per-node permissions (US5) apply only at the RPC layer, never at P2P — this is by design (spec edge case)
- The `keys` param on `publish` is kept for `getBlocksByKeys` backward compatibility
- StreamEntry.hpp is header-only (follows existing pattern: ConsensusConfig.hpp, PeerConfig.hpp)
