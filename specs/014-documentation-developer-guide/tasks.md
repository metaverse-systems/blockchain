# Tasks: Documentation & Developer Guide

**Input**: Design documents from `/specs/014-documentation-developer-guide/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, quickstart.md

**Tests**: Not requested — this is a documentation-only feature with no testable code.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup

**Purpose**: Ensure documentation directory structure is ready

- [X] T001 Verify docs/ directory exists at repository root (already contains ROADMAP.md)

---

## Phase 2: User Story 1 - New Developer Builds from Source (Priority: P1) 🎯 MVP

**Goal**: A new developer can go from `git clone` to a running two-node network by following only the README.

**Independent Test**: Follow the README build steps on a clean machine with only the listed prerequisites installed, producing a working binary and running two connected nodes.

### Implementation for User Story 1

- [X] T002 [US1] Write project description section in README.md (FR-001): what the blockchain node does, key capabilities (PoW consensus, P2P networking, stream-based data, Merkle proofs, TLS everywhere)
- [X] T003 [US1] Write build prerequisites section in README.md (FR-002): compilers with minimum versions (GCC 14, Clang 18), libraries (Boost, OpenSSL, Catch2), and build tools for Linux, macOS, and Windows/MSYS2 — sourced from .github/workflows/ci.yml
- [X] T004 [US1] Write Linux build instructions section in README.md (FR-003): apt-get install, autoreconf, configure, make — sourced from research.md R5
- [X] T005 [US1] Write macOS build instructions section in README.md (FR-003): brew install, PKG_CONFIG_PATH/LDFLAGS/CPPFLAGS exports, autoreconf, configure, make — sourced from research.md R5
- [X] T006 [US1] Write Windows/MSYS2 build instructions section in README.md (FR-003): pacman install, Catch2 from source, autoreconf, configure with --with-boost-libdir, make — sourced from research.md R5
- [X] T007 [US1] Write quickstart section in README.md (FR-004): abbreviated TLS certificate generation steps (link to docs/configuration.md TLS section for full reference), creating two blockchain directories, generating config.json for each with different RPC/P2P ports, starting node 1, starting node 2, connecting via addPeer RPC call, publishing a stream entry on node 1, verifying propagation to node 2
- [X] T008 [US1] Write documentation links section in README.md (FR-013): links to docs/configuration.md, docs/rpc-api.md, docs/architecture.md, docs/contributing.md, and docs/ROADMAP.md
- [X] T009 [US1] Add license section to README.md referencing MIT license

**Checkpoint**: At this point, README.md is complete. A developer can build the project and run a two-node quickstart by following only the README.

---

## Phase 3: User Story 2 - Operator Configures and Runs a Node (Priority: P2)

**Goal**: An operator can look up any CLI flag or config.json field and understand TLS setup from docs/configuration.md.

**Independent Test**: Using only docs/configuration.md, set up a node with custom RPC port, P2P port, log level, and seed peers, then verify the node starts with those settings.

### Implementation for User Story 2

- [X] T010 [US2] Write CLI flags reference table in docs/configuration.md (FR-005): all 9 flags (--help, --version, --config, --rpc-port, --p2p-port, --seed-node, --log-level, --generate-config, positional blockchain-dir) with name, short form, type, default, and description — sourced from specs/009 CLI contract and research.md R6
- [X] T011 [US2] Write CLI precedence rules section in docs/configuration.md (FR-005): CLI flags > config.json > built-in defaults, with exit codes table
- [X] T012 [US2] Write config.json schema reference in docs/configuration.md (FR-006): all 30 fields organized by section (tls, network, consensus, peers, streams, persistence) with JSON key path, type, default value, and description — sourced from research.md R6
- [X] T013 [US2] Write TLS setup section in docs/configuration.md (FR-007): step-by-step OpenSSL commands for generating CA key, CA cert, server key, CSR, signed server cert; explain cert_file, key_file, ca_file config fields; note that both RPC and P2P require TLS
- [X] T014 [US2] Write a complete default config.json example in docs/configuration.md showing all sections with all fields and their defaults

**Checkpoint**: docs/configuration.md is complete. An operator can fully configure a node using only this reference.

---

## Phase 4: User Story 3 - Developer Integrates via RPC API (Priority: P3)

**Goal**: A developer can look up any JSON-RPC method and make a successful call using only docs/rpc-api.md.

**Independent Test**: Using only the documented curl examples, make a successful call to every listed endpoint and verify the response matches the documented format.

### Implementation for User Story 3

- [X] T015 [US3] Write introduction and table of contents in docs/rpc-api.md (FR-008): JSON-RPC 2.0 over TLS, base URL format, 6 domain groups (Streams, Blocks, Peers, Node, Merkle, Sync), pre-1.0 stability caveat
- [X] T016 [US3] Write Streams group in docs/rpc-api.md (FR-008): publish, createStream, listStreams, getStreamEntries, getStreamEntry — each with description, parameters table, request/response JSON, curl --cacert example — sourced from specs/006 contract and RpcServer.cpp
- [X] T017 [US3] Write Blocks group in docs/rpc-api.md (FR-008): getBlockByIndex, getBlocksByKeys, getBlockRange — each with description, parameters table, request/response JSON, curl --cacert example — sourced from specs/001 and 010 contracts and RpcServer.cpp
- [X] T018 [US3] Write Peers group in docs/rpc-api.md (FR-008): addPeer, removePeer, listPeers, banPeer, unbanPeer — each with description, parameters table, request/response JSON, curl --cacert example — sourced from specs/004 contract and RpcServer.cpp
- [X] T019 [US3] Write Node group in docs/rpc-api.md (FR-008): getNodeStatus, getChainLength, getChunkCount — each with description, parameters table, request/response JSON, curl --cacert example — sourced from specs/010 contract and RpcServer.cpp
- [X] T020 [US3] Write Merkle group in docs/rpc-api.md (FR-008): getInclusionProof, verifyInclusionProof, getBlockHeader — each with description, parameters table, request/response JSON, curl --cacert example — sourced from specs/008 contract and RpcServer.cpp
- [X] T021 [US3] Write Sync group in docs/rpc-api.md (FR-008): requestSync — description, parameters table, request/response JSON, curl --cacert example — sourced from specs/003 contract and RpcServer.cpp
- [X] T022 [US3] Write error codes section in docs/rpc-api.md (FR-009): standard JSON-RPC error codes (-32700 parse error, -32600 invalid request, -32601 method not found, -32602 invalid params, -32603 internal error) plus application-specific errors from RpcServer.cpp

**Checkpoint**: docs/rpc-api.md is complete. A developer can integrate with all 20 RPC methods using only this reference.

---

## Phase 5: User Story 4 - Contributor Understands Architecture (Priority: P4)

**Goal**: A contributor reads docs/architecture.md and can describe the four major subsystems and the block propagation data flow.

**Independent Test**: Ask a developer unfamiliar with the codebase to read the architecture overview and correctly describe the role of each major component.

### Implementation for User Story 4

- [X] T023 [US4] Write system overview section with Mermaid block diagram in docs/architecture.md (FR-010): show Consensus Engine, P2P Networking (PeerManager, PeerServer, PeerClient), Persistence Layer (Blockchain, Chunk), and RPC Server as connected subsystems — sourced from plan.md architecture research
- [X] T024 [US4] Write Consensus Engine subsection in docs/architecture.md (FR-010): PoW with leading zero bits, difficulty adjustment algorithm (target interval, adjustment window, log₂ ratio, clamping), mining timeout — sourced from ConsensusConfig.hpp
- [X] T025 [US4] Write P2P Networking subsection in docs/architecture.md (FR-010): dual-port model (RPC on 12345, P2P on 12346), mutual TLS for P2P, peer exchange gossip, ban/reputation system — sourced from PeerManager, PeerServer, PeerClient
- [X] T026 [US4] Write Persistence Layer subsection in docs/architecture.md (FR-010): chunk-based storage (100 blocks per chunk), lazy loading, dirty tracking, recovery with validation, index files (keys.dat, streams.dat, stream_index.dat) — sourced from Chunk.cpp, Blockchain.cpp
- [X] T027 [US4] Write RPC Server subsection in docs/architecture.md (FR-010): JSON-RPC 2.0 over TLS, session handling, sync-awareness (gating during sync) — sourced from RpcServer.cpp
- [X] T028 [US4] Write block mining data flow section with Mermaid sequence diagram in docs/architecture.md (FR-011): RPC publish → StreamEntry creation → Merkle root → PoW mining loop → chunk append → difficulty check — sourced from plan.md architecture research
- [X] T029 [US4] Write block propagation data flow section with Mermaid sequence diagram in docs/architecture.md (FR-011): PeerServer receives block → BlockPropagation dedup/rate-limit → validate → appendBlock → relay to other peers — sourced from plan.md architecture research
- [X] T030 [P] [US4] Write chain sync data flow section with Mermaid sequence diagram in docs/architecture.md (FR-011): PeerClient sends SyncQuery → receives SyncResponse chunks → validates and appends → replaceChain if longer
- [X] T031 [P] [US4] Write stream query data flow section with Mermaid sequence diagram in docs/architecture.md (FR-011): RPC getStreamEntries → stream index lookup → chunk lazy load → return entries

**Checkpoint**: docs/architecture.md is complete. A new contributor can understand the system structure and key data flows.

---

## Phase 6: User Story 5 - Contributor Submits a Change (Priority: P5)

**Goal**: A contributor can build the project, run tests, and understand the PR workflow from docs/contributing.md.

**Independent Test**: Follow the contributing guide to make a trivial change, run the test suite, and verify all expected steps are documented.

### Implementation for User Story 5

- [X] T032 [US5] Write building section in docs/contributing.md (FR-012): link to README.md build instructions (avoid duplication)
- [X] T033 [US5] Write running tests section in docs/contributing.md (FR-012): list all test binaries with commands to run each individually (per constitution § III), explain that `make check` should not be used as a single invocation
- [X] T034 [US5] Write coding conventions section in docs/contributing.md (FR-012): C++20 standard, follow existing style (naming, indentation, brace placement, #pragma once), no formal formatter, commit message guidelines (imperative mood, concise summary line)
- [X] T035 [US5] Write dependencies policy section in docs/contributing.md (FR-012): approved set (Boost, OpenSSL, nlohmann/json, Catch2), adding new dependencies requires explicit approval
- [X] T036 [US5] Write pull request workflow section in docs/contributing.md (FR-012): create feature branch from main, implement with tests, build with make, run each test binary, open PR, merge after review — sourced from constitution § VIII

**Checkpoint**: docs/contributing.md is complete. A contributor has everything needed to submit a change.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final validation and roadmap update

- [X] T037 Verify all documentation links: every docs/*.md file linked from README.md, all cross-document references resolve (FR-013, SC-004)
- [X] T038 Verify completeness: every CLI flag from CliParser.cpp documented in docs/configuration.md (SC-002), every config.json field from NodeConfig.cpp documented (SC-002), every RPC method from RpcServer.cpp documented in docs/rpc-api.md (SC-003)
- [X] T039 Verify all curl examples in docs/rpc-api.md use `--cacert ca.pem` and contain no `-k` or `--insecure` flags
- [X] T040 Update docs/ROADMAP.md: move 014 to Completed table with summary, update "Last updated" date (constitution § XIII)
- [X] T041 Run quickstart.md verification steps from specs/014-documentation-developer-guide/quickstart.md; time the end-to-end walkthrough against the SC-001 target of 15 minutes

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **US1 / README (Phase 2)**: Depends on Phase 1 — this is the MVP
- **US2 / Configuration (Phase 3)**: Can start after Phase 1 — independent of README content
- **US3 / RPC API (Phase 4)**: Can start after Phase 1 — independent of README content
- **US4 / Architecture (Phase 5)**: Can start after Phase 1 — independent of README content
- **US5 / Contributing (Phase 6)**: Can start after Phase 1 — references README build instructions, but can be written in parallel using known content
- **Polish (Phase 7)**: Depends on ALL user stories being complete

### User Story Dependencies

- **US1 (P1)**: No dependencies on other stories — delivers standalone MVP (README.md)
- **US2 (P2)**: Independent of US1 — docs/configuration.md is self-contained
- **US3 (P3)**: Independent of US1/US2 — docs/rpc-api.md is self-contained
- **US4 (P4)**: Independent of US1/US2/US3 — docs/architecture.md is self-contained
- **US5 (P5)**: Weakly depends on US1 (links to build instructions), but can be written in parallel

### Within Each User Story

- Tasks within a story are sequential (each section builds on prior sections in the same file)
- Exception: T030 and T031 are parallel (independent diagram sections)

### Parallel Opportunities

- **After Phase 1**: US1 through US5 can all proceed in parallel (each writes a different file)
- **Within US3 (RPC API)**: T016–T022 write independent method groups — can be parallelized
- **Within US4 (Architecture)**: T030 and T031 are marked [P] (independent data flow sections)
- **Within US3**: All method group tasks (T016–T021) write non-overlapping sections of the same file and can proceed in parallel

---

## Parallel Example: Maximum Parallelism After Phase 1

```
After T001 (Setup) completes:

Worker A: T002-T009 (README.md — US1)
Worker B: T010-T014 (docs/configuration.md — US2)
Worker C: T015-T022 (docs/rpc-api.md — US3)
Worker D: T023-T031 (docs/architecture.md — US4)
Worker E: T032-T036 (docs/contributing.md — US5)

Then: T037-T041 (Polish — requires all above complete)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: User Story 1 (README.md)
3. **STOP and VALIDATE**: Follow the README to build and run a two-node quickstart
4. Delivers immediate value — the project is usable by outsiders

### Incremental Delivery

1. Complete US1 → README.md is complete → Project is buildable and runnable
2. Add US2 → Configuration reference available → Operators can customize nodes
3. Add US3 → RPC API reference available → Developers can integrate
4. Add US4 → Architecture overview available → Contributors understand the system
5. Add US5 → Contributing guide available → Contributors can submit changes
6. Each story adds value without breaking previous stories

### Sequential Strategy (Single Worker)

1. T001 (Setup)
2. T002–T009 (README.md)
3. T010–T014 (docs/configuration.md)
4. T015–T022 (docs/rpc-api.md)
5. T023–T031 (docs/architecture.md)
6. T032–T036 (docs/contributing.md)
7. T037–T041 (Polish)
