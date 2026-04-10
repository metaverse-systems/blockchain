# Tasks: Code Constitution Audit & Remediation

**Input**: Design documents from `/specs/001-code-constitution-audit/`
**Prerequisites**: plan.md (required), spec.md (required), research.md, data-model.md, contracts/

**Tests**: Required by Constitution Principle III ("Every new feature MUST include both unit tests and network integration tests"). Test tasks are included per user story.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Configuration templates needed before implementation begins

- [X] T001 Create .env.example in project root with header comment "# Copy this file to your blockchain data directory as .env", TLS certificate path variables (BLOCKCHAIN_CERT_FILE, BLOCKCHAIN_KEY_FILE, BLOCKCHAIN_CA_FILE), and optional BLOCKCHAIN_TIMEOUT setting

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Shared utility functions that multiple user stories depend on (US2, US5, US7)

**⚠️ CRITICAL**: US2 depends on loadDotEnv(); US5 and US7 depend on logMessage()

- [X] T002 Implement logMessage() structured logging helper (timestamp + severity + message to stderr) in src/utils.hpp and src/utils.cpp per research decision R6
- [X] T003 Implement loadDotEnv() .env file parser (KEY=VALUE with comments, quotes, whitespace handling) in src/utils.hpp and src/utils.cpp per research decision R5

**Checkpoint**: Foundation ready — shared utilities available for all user stories

---

## Phase 3: User Story 1 — Enforce C++20 Compilation Standard (Priority: P1) 🎯 MVP

**Goal**: Build system enforces C++20 across all targets; `configure` validates compiler support

**Independent Test**: Run `./configure && make` from clean checkout — both `blockchain` and `blockchain_tests` must compile under `-std=c++20`. Run `./configure` with a C++17-only compiler and confirm it fails with a clear error.

### Implementation for User Story 1

- [X] T004 [US1] Add AX_CXX_COMPILE_STDCXX(20, noext, mandatory) macro to configure.ac per research decision R1
- [X] T005 [P] [US1] Replace -std=c++17 with -std=c++20 in src/Makefile.am
- [X] T006 [P] [US1] Replace -std=c++17 with -std=c++20 in tests/Makefile.am

**Checkpoint**: Project compiles under C++20 on all targets

---

## Phase 4: User Story 2 — Harden TLS for All Network Communication (Priority: P1)

**Goal**: Certificate paths are configurable via environment variables / .env file; P2P uses mutual TLS; RPC uses server-only TLS

**Independent Test**: Start daemon with custom cert paths via env vars. Verify P2P rejects peers without valid certs. Verify RPC accepts clients without client certs. Verify daemon exits with clear error when cert env vars are missing.

### Tests for User Story 2

> **NOTE: Write these tests FIRST, ensure they FAIL before implementation**

- [X] T007 [P] [US2] Write Catch2 test: daemon startup logic exits with structured error when BLOCKCHAIN_CERT_FILE or BLOCKCHAIN_KEY_FILE env vars are unset, in tests/server_tests.cpp per US2 acceptance scenario 4
- [X] T008 [P] [US2] Write Catch2 test: P2P server with mutual TLS rejects a connecting peer that does not present a valid certificate, in tests/server_tests.cpp per FR-004a

### Implementation for User Story 2

- [X] T009 [US2] Wire loadDotEnv(blockchainDir / ".env") call at startup (where blockchainDir is argv[1]), read BLOCKCHAIN_CERT_FILE, BLOCKCHAIN_KEY_FILE, BLOCKCHAIN_CA_FILE, BLOCKCHAIN_TIMEOUT environment variables, and validate that BLOCKCHAIN_CERT_FILE and BLOCKCHAIN_KEY_FILE are set — if missing, log a structured error via logMessage and exit with non-zero status — in src/main.cpp per FR-003 and US2 acceptance scenario 4
- [X] T010 [US2] Create two separate boost::asio::ssl::context instances — mutual TLS for P2P (verify_peer | verify_fail_if_no_peer_cert + load_verify_file) and server-only for RPC (verify_none on server side) — in src/main.cpp per research decision R4
- [X] T011 [P] [US2] Update PeerServer constructor and Server template instantiation to accept and use the mutual TLS ssl::context in src/network/PeerServer.cpp, src/network/PeerServer.hpp, and src/network/Server.hpp
- [X] T012 [P] [US2] Update RpcServer constructor and Server template instantiation to accept and use the server-only TLS ssl::context in src/network/RpcServer.cpp, src/network/RpcServer.hpp, and src/network/Server.hpp

**Checkpoint**: All network endpoints use properly configured TLS with configurable certificates

---

## Phase 5: User Story 3 — Fix Build-Breaking Bugs (Priority: P1)

**Goal**: Tests compile on case-sensitive filesystems; exceptions use value semantics

**Independent Test**: Run `make check` on Linux (case-sensitive filesystem). Verify test binary compiles without errors. Verify exceptions from Chunk::save() are caught by `catch(const std::runtime_error&)` handlers.

### Tests for User Story 3

> **NOTE: Write this test FIRST, ensure it FAILS before implementation**

- [X] T013 [US3] Write Catch2 test: exception thrown by Chunk::save() (e.g., to a read-only path) is catchable via catch(const std::runtime_error&) in tests/block_tests.cpp per FR-006

### Implementation for User Story 3

- [X] T014 [P] [US3] Fix #include directive filename casing to match actual filenames in tests/server_tests.cpp per FR-005
- [X] T015 [P] [US3] Fix source file name references to match actual filenames in tests/Makefile.am per FR-005
- [X] T016 [P] [US3] Replace throw new std::runtime_error(...) with throw std::runtime_error(...) in src/Chunk.cpp per FR-006

**Checkpoint**: `make check` compiles and runs on case-sensitive filesystems; exceptions are catchable

---

## Phase 6: User Story 4 — Fix Cross-Platform Path Handling (Priority: P2)

**Goal**: All filesystem paths use std::filesystem::path operators instead of string concatenation with `/`

**Independent Test**: Review all path construction in Chunk.cpp and Blockchain.cpp — confirm operator/ is used everywhere. On any platform, verify chunk save/load and key save/load produce correct paths.

### Implementation for User Story 4

- [X] T017 [P] [US4] Replace blockchainPath.string() + "/" + filename string concatenation with blockchainPath / filename in src/Chunk.cpp (save and load methods) per research decision R7
- [X] T018 [P] [US4] Replace blockchainPath.string() + "/" + filename string concatenation with blockchainPath / filename in src/Blockchain.cpp (saveKeys and loadKeys methods) per research decision R7

**Checkpoint**: All filesystem paths use std::filesystem::path::operator/ for cross-platform correctness

---

## Phase 7: User Story 7 — Reduce Duplicate Code in Network Layer (Priority: P3, moved before US5)

**Goal**: SSL handshake and error handling consolidated into SessionHandler base class

**Why moved up**: US7 consolidates handshake code into SessionHandler. US5 adds logging + timeout to handshake code. Doing US7 first means US5 only needs to modify the single shared implementation instead of adding the same code to both RpcServer and PeerServer and then immediately moving it. This eliminates rework and resolves the FR-008/FR-011 overlap.

**Independent Test**: Verify both RpcServer and PeerServer still pass all existing tests after the handshake logic is extracted to SessionHandler.

### Implementation for User Story 7

- [X] T019 [US7] Implement shared SSL async_handshake in SessionHandler::start() with virtual on_handshake_complete() callback in src/network/SessionHandler.hpp per FR-011
- [X] T020 [P] [US7] Refactor RpcServer to remove local handshake code, delegate to SessionHandler::start(), and override on_handshake_complete() for JSON-RPC read loop in src/network/RpcServer.cpp and src/network/RpcServer.hpp
- [X] T021 [P] [US7] Refactor PeerServer to remove local handshake code, delegate to SessionHandler::start(), and override on_handshake_complete() for P2P header read in src/network/PeerServer.cpp and src/network/PeerServer.hpp
- [X] T022 [US7] Update MockSessionHandler to conform to new SessionHandler base interface (virtual on_handshake_complete) in src/network/MockSessionHandler.hpp
- [X] T023 [US7] Update tests/server_tests.cpp for refactored SessionHandler base interface — verify Server construction and start still work with the new virtual method dispatch

**Checkpoint**: Handshake logic exists in one place; both servers delegate to the shared implementation

---

## Phase 8: User Story 5 — Fix Silent SSL Handshake Failures and Add Timeouts (Priority: P2, after US7)

**Goal**: SSL handshake failures in the shared SessionHandler::start() are logged with structured output; stalled async operations time out and are cleaned up

**Why after US7**: The handshake code now lives in SessionHandler::start() (from US7). Logging and timeout are added once to the shared implementation rather than duplicated per-server.

**Independent Test**: Simulate an invalid SSL handshake — verify a structured log entry (timestamp, severity, message) appears on stderr. Simulate a stalled connection — verify it is closed after the configured timeout.

### Tests for User Story 5

> **NOTE: Write this test FIRST, ensure it FAILS before implementation**

- [X] T024 [US5] Write Catch2 test: a connection that stalls beyond the timeout period is closed and a timeout log entry is produced, in tests/server_tests.cpp per FR-009 and SC-005

### Implementation for User Story 5

- [X] T025 [US5] Add steady_timer and timeout_duration members to SessionHandler base class, with helper methods to arm and cancel the timer, in src/network/SessionHandler.hpp per research decision R3
- [X] T026 [US5] Add structured error logging (via logMessage) for SSL handshake failures and wrap async operations with steady_timer timeout in SessionHandler::start() in src/network/SessionHandler.hpp per FR-008 and FR-009

**Checkpoint**: All SSL failures produce structured log output; stalled connections time out after 30 seconds

---

## Phase 9: User Story 6 — Fix Chunk Copy Bug in addBlock (Priority: P2)

**Goal**: addBlock operates on the actual chain vector element, not a temporary copy

**Independent Test**: Add 5 blocks sequentially, then retrieve all 5 by index — verify data integrity. Add 100 blocks to trigger a new chunk — verify block 100 is in the new chunk and blocks 1–99 remain accessible.

### Tests for User Story 6

> **NOTE: Write this test FIRST, ensure it FAILS before implementation**

- [X] T027 [US6] Write Catch2 test: add 5 blocks sequentially, retrieve all by index via getBlockByIndex(), verify data integrity; add 100 blocks to trigger new chunk boundary, verify all remain accessible, in tests/block_tests.cpp per FR-010 and SC-006

### Implementation for User Story 6

- [X] T028 [US6] Change `auto currentChunk = this->chain.back()` to `auto& currentChunk = this->chain.back()` in Blockchain::addBlock() in src/Blockchain.cpp per FR-010

**Checkpoint**: addBlock correctly persists blocks in the chain vector

---

## Phase 10: User Story 8 — Optimize Multi-Key Block Retrieval (Priority: P3)

**Goal**: getBlocksByKeys loads each chunk at most once per query by grouping indices by chunk

**Independent Test**: Query blocks across multiple keys that map to the same chunk — verify chunk is loaded once. Query across N chunks — verify at most N loadChunk calls.

### Tests for User Story 8

> **NOTE: Write this test FIRST, ensure it FAILS before implementation**

- [X] T029 [US8] Write Catch2 test: multi-key query spanning N chunks triggers at most N loadChunk calls (verified via MockChunk load counting), in tests/block_tests.cpp per FR-012 and SC-007

### Implementation for User Story 8

- [X] T030 [US8] Refactor getBlocksByKeys() to collect all block indices, group by chunk index (index / 100), load each chunk once, and extract matching blocks in src/Blockchain.cpp per FR-012

**Checkpoint**: Multi-key queries perform O(chunks) loads instead of O(blocks) loads

---

## Phase 11: Polish & Cross-Cutting Concerns

**Purpose**: Thread safety, documentation, and final validation across all user stories

- [X] T031 Add boost::asio::strand member to Blockchain class template and wrap addBlock, saveChunk, loadChunk, saveKeys, and loadKeys in strand dispatch in src/Blockchain.hpp and src/Blockchain.cpp per FR-013 and research decision R2
- [X] T032 [P] Update README.md with current C++20 build requirements, TLS certificate configuration instructions, .env file usage, and project status
- [X] T033 Run quickstart.md validation — execute the build, configure, TLS setup, run, and test commands from specs/001-code-constitution-audit/quickstart.md and verify they work end-to-end

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS US2 (needs loadDotEnv), US7 (needs logMessage for handshake errors), US5 (needs logMessage)
- **US1 (Phase 3)**: Depends on Setup only — can start in parallel with Foundational
- **US2 (Phase 4)**: Depends on Foundational (T003 loadDotEnv)
- **US3 (Phase 5)**: No dependency on Foundational — can run in parallel with US1 and US2
- **US4 (Phase 6)**: No dependency on Foundational — can run after Setup
- **US7 (Phase 7)**: Depends on Foundational (T002 logMessage) — moved before US5 to consolidate handshake code first
- **US5 (Phase 8)**: Depends on US7 (T019 shared handshake must exist) + Foundational (T002 logMessage)
- **US6 (Phase 9)**: No dependency on Foundational — can run after Setup
- **US8 (Phase 10)**: No dependency on other user stories — can run after Setup
- **Polish (Phase 11)**: Depends on all user stories being complete (T031 wraps methods modified by US6 and US8)

### User Story Dependencies

- **US1 (P1)**: Independent — only modifies build configuration files
- **US2 (P1)**: Depends on Foundational T003 (loadDotEnv utility)
- **US3 (P1)**: Independent — only fixes bugs in test and source files
- **US4 (P2)**: Independent — only changes path construction
- **US7 (P3)**: Depends on Foundational T002 (logMessage for error logging in shared handshake)
- **US5 (P2)**: Depends on US7 T019 (adds timeout/logging to the shared handshake code US7 creates)
- **US6 (P2)**: Independent — single-line reference fix
- **US8 (P3)**: Independent — only changes getBlocksByKeys method

### Within Each User Story

- Tests FIRST — write and verify they FAIL before implementation (Constitution Principle III)
- Foundational utilities before story implementation
- Infrastructure changes (SessionHandler) before server-specific changes
- Core implementation before integration
- Story complete before moving to next priority

### Parallel Opportunities

- **Phase 3 (US1)**: T005 and T006 in parallel (different Makefile.am files)
- **Phase 4 (US2)**: T007+T008 tests in parallel; T011+T012 implementation in parallel (different server files)
- **Phase 5 (US3)**: T014, T015, T016 all in parallel (three different files)
- **Phase 6 (US4)**: T017 and T018 in parallel (different source files)
- **Phase 7 (US7)**: T020 and T021 in parallel (different server files)
- **Phase 11 (Polish)**: T031 and T032 in parallel (different files)
- **Cross-story**: US1, US3, US4, US6, US8 have no inter-story dependencies and could run in parallel after Foundational

---

## Parallel Example: Cross-Story Independence

```bash
# After Foundational phase completes, these stories can start simultaneously:
Story US1: T004→T005+T006          (build config only)
Story US3: T013→T014+T015+T016     (bug fixes)
Story US4: T017+T018               (path fixes only)
Story US6: T027→T028               (reference fix)
Story US8: T029→T030               (query optimization)

# These stories have Foundational dependencies and must wait:
Story US2: Needs T003 (loadDotEnv)
Story US7: Needs T002 (logMessage)

# This story depends on another story:
Story US5: Needs US7 complete (T019 shared handshake)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001)
2. Complete Phase 2: Foundational (T002, T003)
3. Complete Phase 3: User Story 1 — C++20 enforcement (T004, T005, T006)
4. **STOP and VALIDATE**: Run `./configure && make` — verify C++20 compilation
5. The codebase now compiles under the constitutionally mandated standard

### Incremental Delivery

1. Setup + Foundational → Shared utilities ready
2. US1 (C++20) → Build system corrected → validate
3. US2 (TLS hardening) → Secure certificate configuration → validate
4. US3 (Bug fixes) → Tests compile, exceptions catchable → validate with `make check`
5. US4 (Paths) → Cross-platform path handling → validate
6. US7 (Dedup) → Clean network layer, shared handshake → validate
7. US5 (Logging + timeouts) → Network resilience added to shared code → validate
8. US6 (Copy bug) → Data integrity → validate with block insertion test
9. US8 (Optimization) → Efficient queries → validate
10. Polish → Thread safety, documentation, final validation

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup + Foundational together
2. Once Foundational is done:
   - Developer A: US1 + US2 → then US7 → then US5 (build + TLS + network chain)
   - Developer B: US3 + US4 + US6 (bug fixes, independent)
   - Developer C: US8 (optimization, independent)
3. After all stories: Polish phase

---

## Notes

- [P] tasks = different files, no dependencies on incomplete tasks
- [Story] label maps task to specific user story for traceability
- Each user story is independently completable and testable
- Tests are written FIRST and must FAIL before implementation (Constitution Principle III)
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- US7 is P3 but ordered before US5 (P2) to avoid rework — consolidate handshake first, then enhance
- T031 (strand) is in Polish because it wraps methods modified by US6 and US8
