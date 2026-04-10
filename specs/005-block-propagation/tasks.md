# Tasks: Block Propagation & Validation on Receipt

**Input**: Design documents from `/specs/005-block-propagation/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: Build system changes and new file scaffolding

- [ ] T001 Add `BlockPropagation.cpp` to `src/Makefile.am` source list and `block_propagation_tests.cpp` to `tests/Makefile.am` test binary

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core BlockPropagation class with internal data structures that all user stories depend on

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T002 Create `BlockPropagation.hpp` in `src/BlockPropagation.hpp` with class declaration: constructor taking `IBlockchain&`, `SyncStatus&`, and a relay callback `std::function<void(const Block&, const std::string&)>`; declare `on_block_received(const Block&, const std::string& sender_key)` and `process_sync_queue()` public methods; declare private members for RecentBlockCache (`std::unordered_set<std::string>` + `std::deque<std::string>`, max 512), PendingBlock pool (`std::unordered_map<std::string, PendingBlock>`, max 64, 60s TTL), SyncBlockQueue (`std::deque<std::pair<Block, std::string>>`, max 128), and per-peer BlockRateState map (`std::unordered_map<std::string, BlockRateState>`); define `PendingBlock` and `BlockRateState` structs per data-model.md
- [ ] T003 Create `BlockPropagation.cpp` in `src/BlockPropagation.cpp` with constructor implementation storing references and relay callback; implement empty stubs for `on_block_received()` and `process_sync_queue()` that compile and link
- [ ] T026 Add inbound session tracking to `PeerManager` in `src/PeerManager.hpp` and `src/PeerManager.cpp`: add a `std::map<std::string, std::weak_ptr<PeerServer>> inbound_sessions_` member; in `on_inbound_connected()` store the session `shared_ptr` (passed from `Server::start_accept()`); in `on_inbound_disconnected()` remove the entry. Update `Server` template in `src/network/Server.hpp` to pass the session `shared_ptr` to PeerManager on accept
- [ ] T027 Add `virtual void appendBlock(const Block &block) = 0` to `IBlockchain` in `src/IBlockchain.hpp`; implement in `Blockchain` in `src/Blockchain.hpp` and `src/Blockchain.cpp` to insert a pre-mined block at the correct chunk position, save the chunk, and update difficulty — without running the mining loop. Mirrors `addBlock()` logic after the mining loop

**Checkpoint**: BlockPropagation compiles and links; PeerManager tracks inbound sessions; IBlockchain has appendBlock(); stubs are callable from tests

---

## Phase 3: User Story 1 — Locally Created Blocks Are Broadcast to Peers (Priority: P1) 🎯 MVP

**Goal**: When a node creates a block via `addBlock` RPC, it is automatically sent to all connected peers

**Independent Test**: Create a block on Node A while Node B is connected; verify Node B receives it

### Implementation for User Story 1

- [ ] T004 [US1] Add `broadcast_block(const Block &block)` method to `PeerManager` in `src/PeerManager.hpp` and `src/PeerManager.cpp` that iterates all outbound connections (`outbound_connections_`) and all tracked inbound sessions (`inbound_sessions_`) and sends the block via `PacketType::BLOCK` to each connected peer
- [ ] T005 [US1] Modify `RpcServer::do_read()` addBlock handler in `src/network/RpcServer.cpp` to call `peer_manager->broadcast_block(block)` after successful block creation and chunk/key save, before writing the RPC response

**Checkpoint**: A block created via RPC is broadcast to all connected peers. US1 acceptance scenarios 1–3 satisfied.

---

## Phase 4: User Story 2 — Received Blocks Are Validated Before Acceptance (Priority: P1)

**Goal**: Blocks received from peers are validated against consensus rules and appended if valid; invalid blocks are rejected

**Independent Test**: Send a valid and an invalid block to a node; verify only the valid one is appended

### Implementation for User Story 2

- [ ] T006 [US2] Implement the RecentBlockCache private methods in `src/BlockPropagation.cpp`: `cache_contains(const std::string &hash)` returning bool with O(1) lookup, and `cache_insert(const std::string &hash)` that inserts into the set and deque with FIFO eviction when capacity (512) is reached
- [ ] T007 [US2] Implement per-peer rate limiting private method in `src/BlockPropagation.cpp`: `check_rate_limit(const std::string &sender_key)` returning bool (true if allowed); uses lazy 1-second sliding window with limit of 10 blocks/sec per research R3
- [ ] T008 [US2] Implement the core `on_block_received()` method in `src/BlockPropagation.cpp`: check rate limit (drop + log if exceeded), check dedup cache (discard silently if seen), check sync status (enqueue if syncing), validate via `IBlockchain::isValidNewBlock()` against the current chain tip obtained from `bc.getBlockByIndex(bc.getChainBlockCount() - 1)` with `ConsensusConfig`, append valid block via `bc.appendBlock(block)`, add hash to dedup cache, invoke relay callback; for invalid blocks log and return without appending
- [ ] T009 [US2] Implement `appendReceivedBlock()` private helper in `src/BlockPropagation.cpp` that calls `bc.appendBlock(block)` (the new IBlockchain method from T027) to insert the pre-validated block without re-mining, then calls `bc.saveChunk()` and `bc.saveKeys()`
- [ ] T010 [US2] Modify `PeerServer::do_read_body()` BLOCK case in `src/network/PeerServer.cpp` to call `block_propagation->on_block_received(block, sender_key)` instead of just logging; construct sender_key from `remote_host()` and `remote_port()`; add `BlockPropagation*` member and setter to PeerServer
- [ ] T011 [US2] Modify `PeerClient::do_read_body()` BLOCK case in `src/network/PeerClient.cpp` to call `block_propagation->on_block_received(block, sender_key)` instead of just logging; construct sender_key from `host` and `port`; add `BlockPropagation*` member and setter to PeerClient
- [ ] T012 [US2] Wire `BlockPropagation` into `main()` in `src/main.cpp`: construct after `Blockchain` and `PeerManager`, pass relay callback that calls `PeerManager::relay_block()`, set `BlockPropagation*` on PeerServer and PeerClient via PeerManager or Server template

**Checkpoint**: Received blocks are validated and appended or rejected. US2 acceptance scenarios 1–5 satisfied.

---

## Phase 5: User Story 3 — Valid Blocks Are Relayed to Other Peers (Priority: P2)

**Goal**: After validating and accepting a block from one peer, forward it to all other connected peers (excluding sender)

**Independent Test**: Three nodes A→B→C; create block on A; verify C receives via B

### Implementation for User Story 3

- [ ] T013 [US3] Add `relay_block(const Block &block, const std::string &exclude_key)` method to `PeerManager` in `src/PeerManager.hpp` and `src/PeerManager.cpp` that sends the block to all connected outbound peers and all tracked inbound sessions except the one matching `exclude_key`
- [ ] T014 [US3] Update the relay callback wired in `main()` (`src/main.cpp`) to call `peer_manager.relay_block(block, exclude_key)` so that `BlockPropagation::on_block_received()` triggers relay after successful append

**Checkpoint**: Blocks propagate through multi-hop relay. US3 acceptance scenarios 1–3 satisfied.

---

## Phase 6: User Story 4 — Duplicate Blocks Are Suppressed (Priority: P2)

**Goal**: Blocks already seen (via dedup cache or already in chain) are silently discarded

**Independent Test**: Send the same block from two peers; verify it appears once in the chain

### Implementation for User Story 4

- [ ] T015 [P] [US4] Add chain-tip index check at the start of `on_block_received()` in `src/BlockPropagation.cpp`: if `block.index < bc.getChainBlockCount()`, the block is already in the chain — discard silently (covers blocks that aged out of the dedup cache but exist on chain)
- [ ] T016 [US4] Verify dedup cache integration in `on_block_received()` in `src/BlockPropagation.cpp`: confirm that after a block is appended the hash is inserted into the cache before relay, and that duplicate arrivals hit the cache check and return early without relay

**Checkpoint**: Duplicate blocks never double-append or double-relay. US4 acceptance scenarios 1–3 satisfied.

---

## Phase 7: User Story 5 — Misbehaving Peers Are Penalized (Priority: P3)

**Goal**: Peers sending invalid blocks get error counts incremented; existing ban infrastructure handles escalation

**Independent Test**: Send multiple invalid blocks; verify error count increases and ban triggers

### Implementation for User Story 5

- [ ] T017 [US5] Add `PeerManager*` member to `BlockPropagation` in `src/BlockPropagation.hpp` (set via constructor or setter); in `on_block_received()` in `src/BlockPropagation.cpp`, call `peer_manager->increment_error(host, port)` when validation fails or rate limit is exceeded; parse host and port from sender_key
- [ ] T018 [US5] Handle deserialization failures in BLOCK case of `PeerServer::do_read_body()` in `src/network/PeerServer.cpp` and `PeerClient::do_read_body()` in `src/network/PeerClient.cpp`: wrap block deserialization in try/catch, increment peer error on failure

**Checkpoint**: Invalid block senders are penalized. US5 acceptance scenarios 1–2 satisfied.

---

## Phase 8: Pending Pool & Sync Queue (Cross-Cutting)

**Purpose**: Gap-block deferral and sync-aware queueing — supports FR-009 and FR-012

- [ ] T019 Implement pending pool logic in `src/BlockPropagation.cpp`: `defer_block(const Block&, const std::string& sender_key)` inserts into `pending_pool_` keyed by `block.prevHash`; `resolve_pending(const std::string& new_block_hash)` checks pool for entries matching `new_block_hash`, removes and runs `on_block_received()` recursively (with cascade); `evict_expired()` lazily removes entries older than 60s TTL; call `resolve_pending()` after every successful append; call `evict_expired()` on each insertion when pool is full
- [ ] T020 Implement `process_sync_queue()` in `src/BlockPropagation.cpp`: iterate `sync_queue_`, for each entry re-check dedup cache then call the core validation/append/relay flow; clear queue after processing; wire call-site in `PeerClient::handle_sync_response()` in `src/network/PeerClient.cpp` to call `block_propagation->process_sync_queue()` when sync completes (`sync_status.isSyncing` transitions to false)
- [ ] T021 Update `on_block_received()` in `src/BlockPropagation.cpp` to route gap blocks (where `block.prevHash` does not match the current chain tip's hash) into `defer_block()` instead of rejecting

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Tests, validation, cleanup

- [ ] T022 [P] Create `tests/block_propagation_tests.cpp` with Catch2 unit tests: test RecentBlockCache insert/contains/eviction at capacity; test BlockRateState allows up to limit then rejects; test PendingBlock pool insert/resolve/TTL eviction; test SyncBlockQueue enqueue/process; test `on_block_received()` with valid block (mock blockchain, verify append called); test `on_block_received()` with invalid block (verify not appended, error incremented); test duplicate block (verify discarded silently)
- [ ] T028 [P] Create `src/MockBlockchain.hpp` implementing `IBlockchain` with deterministic stubs: `appendBlock()` records the block in a `std::vector<Block>`, `addBlock()` returns a pre-built block, `getBlockByIndex()` / `getChainBlockCount()` return controlled values. Use in `block_propagation_tests.cpp` and `block_propagation_integration_tests.cpp` for test isolation per constitution §III
- [ ] T023 [P] Update `tests/Makefile.am` to build and link `block_propagation_tests` and `block_propagation_integration_tests` binaries with `BlockPropagation.cpp` and required dependencies
- [ ] T029 Create `tests/block_propagation_integration_tests.cpp` with Catch2 integration tests runnable via `make check`: test the full pipeline — BlockPropagation + MockBlockchain + PeerManager (with mock network) — for multi-component scenarios: (a) valid block received → validated → appended → relayed, (b) invalid block → rejected → error incremented → not relayed, (c) duplicate block → dedup cache hit → discarded, (d) gap block → deferred in pending pool → resolved when predecessor arrives. Uses MockBlockchain and mock session handlers for deterministic control
- [ ] T030 Add a throughput benchmark test case in `tests/block_propagation_tests.cpp` (or `block_propagation_integration_tests.cpp`): feed 100 valid blocks through `on_block_received()` in a tight loop using MockBlockchain, measure wall-clock time, assert completion within 10 seconds (validates SC-007: ≥10 blocks/sec)
- [ ] T024 Run `make check` to verify all existing and new tests pass (including integration tests and throughput benchmark)
- [ ] T025 Run quickstart.md two-node validation scenario to confirm end-to-end block propagation

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — BLOCKS all user stories. Includes T026 (inbound session tracking) and T027 (appendBlock method)
- **US1 (Phase 3)**: Depends on Phase 2 (including T026) — broadcast to all peers including inbound
- **US2 (Phase 4)**: Depends on Phase 2 (including T027) — core validation uses appendBlock()
- **US3 (Phase 5)**: Depends on US2 (Phase 4) and T026 — relay requires validation + inbound session access
- **US4 (Phase 6)**: Depends on US2 (Phase 4) — dedup is part of reception flow
- **US5 (Phase 7)**: Depends on US2 (Phase 4) — penalization hooks into validation failure path
- **Pending Pool & Sync Queue (Phase 8)**: Depends on US2 (Phase 4) — extends the reception flow
- **Polish (Phase 9)**: Depends on all prior phases. Includes T028 (MockBlockchain), T029 (integration tests), T030 (throughput benchmark)

### User Story Dependencies

- **US1 (P1)**: Independent — can start after Foundational
- **US2 (P1)**: Independent — can start after Foundational; can run in parallel with US1
- **US3 (P2)**: Depends on US2 (needs validated blocks to relay)
- **US4 (P2)**: Depends on US2 (dedup is part of validation flow)
- **US5 (P3)**: Depends on US2 (error counting hooks into validation)
- **Phase 8**: Depends on US2 (extends the core reception method)

### Parallel Opportunities

```
After Phase 2 (Foundational):
  ┌─ US1 (T004–T005) ─────────────────────────┐
  │                                             │
  └─ US2 (T006–T012) ─┬─ US3 (T013–T014) ─────┤
                       ├─ US4 (T015–T016)       │
                       ├─ US5 (T017–T018)       │
                       └─ Phase 8 (T019–T021) ──┘
                                                │
                                   Phase 9 (T022–T025, T028–T030)
```

US1 and US2 can proceed in parallel since US1 modifies RpcServer/PeerManager (outbound) while US2 modifies BlockPropagation/PeerServer/PeerClient (inbound).

---

## Implementation Strategy

### MVP First (User Story 1 + User Story 2)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (class scaffolding)
3. Complete Phase 3: US1 (broadcast) — blocks are now sent to peers
4. Complete Phase 4: US2 (validation) — received blocks are validated and appended
5. **STOP and VALIDATE**: Two-node test from quickstart.md

### Incremental Delivery

1. Setup + Foundational → Scaffolding ready
2. US1 + US2 → Two-node propagation works (MVP)
3. US3 → Multi-hop relay works
4. US4 → Dedup prevents redundant processing
5. US5 → Bad actors penalized
6. Phase 8 → Gap blocks deferred, sync-safe queueing
7. Phase 9 → Full test coverage (unit + integration + throughput benchmark), validation
