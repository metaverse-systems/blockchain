# Tasks: Peer Discovery & Management

**Input**: Design documents from `/specs/004-peer-discovery/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/json-rpc.md, contracts/p2p-binary.md

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Exact file paths included in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization, config system, and shared data types

- [X] T001 Create PeerAddress struct with JSON and Boost.Serialization support in src/PeerConfig.hpp
- [X] T002 [P] Create PeerEntry struct with JSON serialization in src/PeerConfig.hpp
- [X] T003 [P] Create BanRecord struct with JSON serialization in src/PeerConfig.hpp
- [X] T004 [P] Create PeerConfig struct (peer discovery settings subset) in src/PeerConfig.hpp
- [X] T005 Create NodeConfig class that loads config.json with all sections (tls, network, consensus, peers) in src/NodeConfig.hpp and src/NodeConfig.cpp
- [X] T006 Implement NodeConfig validation rules (port ranges, non-empty TLS paths, peer config constraints) in src/NodeConfig.cpp
- [X] T007 Implement default config.json generation when file does not exist in src/NodeConfig.cpp
- [X] T008 Add generate_uuid_v4() utility function using std::random in src/utils.hpp and src/utils.cpp
- [X] T009 [P] Add PEER_EXCHANGE and PEER_EXCHANGE_RESPONSE to PacketType enum in src/network/PacketHeader.hpp
- [X] T010 [P] Create PeerExchangeRequest and PeerExchangeResponse structs with Boost.Serialization in src/network/PeerMessages.hpp
- [X] T011 Remove loadDotEnv declaration from src/utils.hpp and implementation from src/utils.cpp
- [X] T012 Update src/Makefile.am to add NodeConfig.cpp and PeerManager.cpp to blockchain_SOURCES
- [X] T013 Add Catch2 test file for NodeConfig (load, validate, default generation) in tests/node_config_tests.cpp
- [X] T014 Update tests/Makefile.am to build and link new test files

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: PeerManager core and main.cpp migration — MUST complete before user story work

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [X] T015 Create PeerManager class skeleton with constructor taking io_context, ssl_context, PeerConfig, and data directory path in src/PeerManager.hpp and src/PeerManager.cpp
- [X] T016 Implement PeerManager::load_peers() to read peers.json (or generate UUID on first run); handle malformed JSON by logging a warning and starting with an empty peer list in src/PeerManager.cpp
- [X] T017 Implement PeerManager::save_peers() with atomic write (write temp file, rename; on Windows use platform API behind preprocessor guard for atomic replace) in src/PeerManager.cpp
- [X] T018 Implement PeerManager peer list management: add_peer(), remove_peer(), get_peers(), find_peer() with 256-entry cap and oldest-seen eviction in src/PeerManager.cpp
- [X] T019 Implement PeerManager self-filtering: filter_self() discards own UUID/address from peer lists in src/PeerManager.cpp
- [X] T020 Refactor src/main.cpp to load config.json via NodeConfig instead of .env, use NodeConfig for TLS paths, ports, timeout, and ConsensusConfig construction
- [X] T021 Refactor src/main.cpp to create PeerManager and pass it to Server and RpcServer (replace hardcoded port values with NodeConfig values)
- [X] T022 Add PeerManager reference to PeerServer constructor and PeerClient constructor in src/network/PeerServer.hpp, src/network/PeerServer.cpp, src/network/PeerClient.hpp, src/network/PeerClient.cpp
- [X] T023 Add PeerManager reference to RpcServer constructor in src/network/RpcServer.hpp and src/network/RpcServer.cpp
- [X] T024 Add Catch2 test file for PeerManager core (load/save peers.json, add/remove/cap eviction, self-filter, UUID generation, IPv6 PeerAddress round-trip through JSON and Boost.Serialization) in tests/peer_manager_tests.cpp

**Checkpoint**: Foundation ready — PeerManager owns peer state, config.json replaces .env, all components wired

---

## Phase 3: User Story 1 — Seed Node Bootstrap (Priority: P1) 🎯 MVP

**Goal**: A new node connects to configured seed nodes and obtains the initial peer list

**Independent Test**: Start two nodes — one as seed, one as joiner — verify the joiner connects and receives a peer list

### Implementation for User Story 1

- [X] T025 [US1] Implement PeerManager::start() to connect to seed nodes on startup (create PeerClient per seed, respect max_outbound) in src/PeerManager.cpp
- [X] T026 [US1] Modify PeerClient to send PEER_EXCHANGE immediately after TLS handshake completes (before sync) in src/network/PeerClient.cpp
- [X] T027 [US1] Implement PeerServer handler for PEER_EXCHANGE packet type: deserialize PeerExchangeRequest, call PeerManager to merge peers, send PEER_EXCHANGE_RESPONSE in src/network/PeerServer.cpp
- [X] T028 [US1] Implement PeerClient handler for PEER_EXCHANGE_RESPONSE packet type: deserialize PeerExchangeResponse, call PeerManager to record remote UUID and merge peers in src/network/PeerClient.cpp
- [X] T029 [US1] Implement PeerManager::on_peer_exchange_received() to merge received peer list (filter self, filter banned, enforce cap, connect to new peers if under limit) in src/PeerManager.cpp
- [X] T030 [US1] Wire PeerManager::start() call from main.cpp after io_context setup in src/main.cpp
- [X] T031 [US1] Implement PeerManager::connect_to() that creates a PeerClient, tracks it in outbound_connections, and initiates connection in src/PeerManager.cpp
- [X] T032 [US1] Add Catch2 tests for seed bootstrap flow (connect to seed, receive peers, merge into list) in tests/peer_discovery_tests.cpp

**Checkpoint**: A node can join the network via seed nodes and receive a peer list

---

## Phase 4: User Story 2 — Peer Exchange (Priority: P1)

**Goal**: Connected nodes periodically share full peer lists so the network self-discovers

**Independent Test**: Three nodes (A→B→C); after exchange, A discovers C without direct configuration

### Implementation for User Story 2

- [X] T033 [US2] Implement PeerManager exchange timer (boost::asio::steady_timer, fires every exchange_interval_seconds) in src/PeerManager.cpp
- [X] T034 [US2] Implement PeerManager::broadcast_peer_exchange() to send PEER_EXCHANGE on all active outbound connections in src/PeerManager.cpp
- [X] T035 [US2] Implement PeerManager::on_new_peers_discovered() to attempt outbound connections to newly discovered peers (respect max_outbound, skip banned/self/already-connected) in src/PeerManager.cpp
- [X] T036 [US2] Implement duplicate connection detection: on receiving UUID, check for existing connection with same UUID, resolve by lower-UUID-keeps-outbound rule in src/PeerManager.cpp
- [X] T037 [US2] Call PeerManager::save_peers() after each successful peer exchange merge in src/PeerManager.cpp
- [X] T038 [US2] Add Catch2 tests for periodic exchange and three-node gossip discovery in tests/peer_discovery_tests.cpp

**Checkpoint**: Full peer gossip works — any node can discover all others through transitive exchange

---

## Phase 5: User Story 3 — Connection Limits & Health (Priority: P2)

**Goal**: Node enforces max inbound/outbound connection counts and replaces unhealthy peers

**Independent Test**: Configure max_outbound=3, present 10 peers, verify exactly 3 connections

### Implementation for User Story 3

- [X] T039 [US3] Implement PeerManager::can_accept_inbound() check and integrate into Server<PeerServer> accept loop to reject connections at limit in src/PeerManager.cpp and src/network/PeerServer.hpp
- [X] T040 [US3] Enforce max_outbound in PeerManager::connect_to() — return error if at limit in src/PeerManager.cpp
- [X] T041 [US3] Implement PeerManager::on_peer_disconnected() to update peer entry, decrement connection count, and attempt replacement connection to a known disconnected peer in src/PeerManager.cpp
- [X] T042 [US3] Wire disconnection callbacks from PeerClient and PeerServer read/write error handlers to PeerManager::on_peer_disconnected() in src/network/PeerClient.cpp and src/network/PeerServer.cpp
- [X] T043 [US3] Add Catch2 tests for connection limits (outbound cap, inbound rejection, replacement on disconnect) in tests/peer_manager_tests.cpp

**Checkpoint**: Connection limits enforced, unhealthy connections replaced

---

## Phase 6: User Story 4 — Reconnection with Backoff (Priority: P2)

**Goal**: Dropped connections are automatically retried with exponential backoff and jitter

**Independent Test**: Connect two nodes, kill one, observe increasing retry intervals on survivor

### Implementation for User Story 4

- [X] T044 [P] [US4] Add per-peer backoff state (current_delay, timer) to PeerManager's internal tracking in src/PeerManager.hpp
- [X] T045 [US4] Implement PeerManager::schedule_reconnect() with exponential backoff (double delay each attempt, cap at max, ±20% jitter, reset on success) in src/PeerManager.cpp
- [X] T046 [US4] Call schedule_reconnect() from on_peer_disconnected() for non-banned peers in src/PeerManager.cpp
- [X] T047 [US4] Implement PeerManager backoff reset and error_count reset on successful connection in src/PeerManager.cpp
- [X] T048 [US4] Add Catch2 tests for backoff progression (base→double→cap, jitter range, reset) in tests/peer_manager_tests.cpp

**Checkpoint**: Network is self-healing — dropped connections automatically retry with appropriate pacing

---

## Phase 7: User Story 5 — Manual Peer Management via RPC (Priority: P2)

**Goal**: Operators can add/remove/list peers via RPC, and disable automatic discovery

**Independent Test**: Start node with discovery disabled, add peer via RPC, verify connection

### Implementation for User Story 5

- [X] T049 [US5] Implement addPeer RPC handler in src/network/RpcServer.cpp (validate params, check ban/limit, call PeerManager::connect_to)
- [X] T050 [US5] Implement removePeer RPC handler in src/network/RpcServer.cpp (validate params, call PeerManager::disconnect_and_remove)
- [X] T051 [US5] Implement listPeers RPC handler in src/network/RpcServer.cpp (return node_uuid, connection counts, peer list with status, ban list)
- [X] T052 [US5] Implement discovery_enabled gate: when false, PeerManager skips seed connections and periodic exchange but still accepts manual addPeer in src/PeerManager.cpp
- [X] T053 [US5] Add static RPC error helpers for new error codes (-32003 through -32006) in src/network/RpcServer.hpp and src/network/RpcServer.cpp
- [X] T054 [US5] Add Catch2 tests for RPC methods (addPeer, removePeer, listPeers, discovery_enabled toggle) in tests/server_tests.cpp

**Checkpoint**: Operators have full manual control over peer connections

---

## Phase 8: User Story 6 — Peer Ban & Reputation (Priority: P3)

**Goal**: Misbehaving peers are tracked and automatically/manually banned

**Independent Test**: Simulate peer sending malformed data past threshold, verify auto-ban

### Implementation for User Story 6

- [X] T055 [US6] Implement PeerManager::increment_error() to track consecutive errors per peer and trigger auto-ban when threshold exceeded in src/PeerManager.cpp
- [X] T056 [US6] Implement PeerManager::ban_peer() and unban_peer() for manual ban management in src/PeerManager.cpp
- [X] T057 [US6] Implement PeerManager::is_banned() check and purge_expired_bans() on startup and periodically in src/PeerManager.cpp
- [X] T058 [US6] Wire error counting into PeerServer and PeerClient: increment error count on deserialization failures and protocol violations in src/network/PeerServer.cpp and src/network/PeerClient.cpp
- [X] T059 [US6] Integrate ban check into PeerServer post-handshake (reject banned inbound peers) in src/network/PeerServer.cpp
- [X] T060 [US6] Implement banPeer RPC handler in src/network/RpcServer.cpp (validate params, call PeerManager::ban_peer, disconnect if connected)
- [X] T061 [US6] Implement unbanPeer RPC handler in src/network/RpcServer.cpp (validate params, call PeerManager::unban_peer)
- [X] T062 [US6] Add Catch2 tests for ban/reputation (auto-ban threshold, manual ban/unban, expired ban cleanup, banned peer rejection) in tests/peer_manager_tests.cpp

**Checkpoint**: Network is protected from misbehaving peers

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Shutdown handling, build integration, and final validation

- [X] T063 [P] Wire PeerManager::save_peers() into SIGINT/SIGTERM handler in src/main.cpp for graceful shutdown persistence
- [X] T064 [P] Audit tests/Makefile.am to verify all new test files (node_config_tests, peer_manager_tests, peer_discovery_tests) compile and link with `make check`
- [X] T065 Verify `make check` passes with all new and existing tests
- [X] T066 Run quickstart.md Scenario 1 (seed bootstrap) and Scenario 2 (three-node gossip) end-to-end validation

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 completion — BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Phase 2 — first user story, MVP
- **US2 (Phase 4)**: Depends on US1 (needs working peer exchange handling)
- **US3 (Phase 5)**: Depends on Phase 2 — can run in parallel with US1/US2
- **US4 (Phase 6)**: Depends on US3 (needs on_peer_disconnected wiring)
- **US5 (Phase 7)**: Depends on Phase 2 — can run in parallel with US1/US2/US3
- **US6 (Phase 8)**: Depends on Phase 2 — can run in parallel with other stories
- **Polish (Phase 9)**: Depends on all user stories complete

### User Story Independence

- **US1 + US2** are sequential (US2 builds on US1's exchange handling)
- **US3, US5, US6** can proceed in parallel with US1/US2 (after Phase 2)
- **US4** requires US3's disconnection wiring

### Parallel Opportunities per Phase

**Phase 1**: T001-T004 in parallel (structs), T009-T010 in parallel (packet types), T013-T014 in parallel (test setup)

**Phase 3 (US1)**: T026-T028 in parallel (PeerClient send, PeerServer receive, PeerClient receive)

**Phase 5 + 7 + 8 (US3 + US5 + US6)**: These three user stories can all proceed in parallel after Phase 2

---

## Implementation Strategy

### MVP: Phase 1 → Phase 2 → Phase 3 (US1)

Delivers: config.json, PeerManager, seed node connection, initial peer exchange. A node can join the network.

### Increment 2: Phase 4 (US2)

Delivers: periodic gossip. The network self-discovers without every node needing seed configuration.

### Increment 3: Phase 5 + 6 + 7 + 8 (US3 + US4 + US5 + US6) — parallel

Delivers: connection limits, reconnection, RPC management, ban/reputation. Production-ready peer management.

### Final: Phase 9

Delivers: shutdown persistence, build integration, end-to-end validation.
