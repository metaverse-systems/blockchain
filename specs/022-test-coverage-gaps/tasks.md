---

description: "Task list for peer disconnect test coverage"
---

# Tasks: Peer Disconnect Test Coverage

**Input**: Design documents from `/specs/022-test-coverage-gaps/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: This feature IS test coverage — all tasks are test tasks.

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2)
- Include exact file paths in descriptions

## Path Conventions

- **Single project**: `src/`, `tests/` at repository root
- This feature adds tests to existing test binaries — no new files or build targets

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Verify existing test infrastructure is sufficient for new test cases

- [X] T001 Confirm tests/block_propagation_tests.cpp includes MockBlockchain.hpp and TestHelpers.hpp, and tests/peer_manager_tests.cpp includes MockChunk.hpp — verify existing test infrastructure is sufficient for new test cases

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: No foundational tasks required — existing test infrastructure (MockBlockchain, MockChunk, TestHelpers) is sufficient for all test scenarios.

**Checkpoint**: Foundation ready — existing test files and utilities are in place.

---

## Phase 3: User Story 1 - Test peer disconnect during outbound block propagation (Priority: P1) 🎯 MVP

**Goal**: Add tests that verify relay callback exceptions during block propagation are handled gracefully — block propagation continues to remaining peers when one peer disconnects mid-relay.

**Independent Test**: `./tests/block_propagation_tests "[relay_exception]"` — all relay exception tests pass without crash.

### Implementation for User Story 1

- [X] T002 [US1] Add relay callback exception test in tests/block_propagation_tests.cpp — verify block is appended even when relay callback throws
- [X] T003 [US1] Add relay callback selective failure test in tests/block_propagation_tests.cpp — verify relay continues to remaining peers when one peer throws
- [X] T004 [US1] Add multiple relay failure test in tests/block_propagation_tests.cpp — verify node doesn't crash when relay callback throws for all peers
- [X] T005 [US1] Add outbound peer disconnect error count test in tests/peer_manager_tests.cpp — verify on_peer_disconnected increments error_count
- [X] T006 [US1] Add outbound peer disconnect reconnect scheduling test in tests/peer_manager_tests.cpp — verify non-banned peer gets reconnect scheduled via on_peer_disconnected
- [X] T007 [US1] Add outbound peer disconnect skip reconnect when banned test in tests/peer_manager_tests.cpp — verify banned peer does not get reconnect scheduled

**Checkpoint**: User Story 1 is fully functional — relay exception handling and outbound disconnect are covered by tests.

---

## Phase 4: User Story 2 - Test inbound peer disconnect during block reception (Priority: P2)

**Goal**: Add tests that verify inbound peer disconnect correctly removes session state and decrements inbound connection count.

**Independent Test**: `./tests/peer_manager_tests "[disconnect]"` — all disconnect handler tests pass.

### Implementation for User Story 2

- [X] T008 [P] [US2] Add inbound peer disconnect session removal test in tests/peer_manager_tests.cpp — verify on_inbound_disconnected removes session and decrements inbound_count
- [X] T009 [P] [US2] Add outbound peer disconnect inbound dedup test in tests/peer_manager_tests.cpp — verify outbound disconnect skips reconnect when inbound session from same UUID exists (addresses edge case: "peer disconnects but inbound session from same node is still alive")

**Checkpoint**: User Stories 1 AND 2 are both complete — all medium-severity coverage gaps from AUDIT.md §7.5 are resolved.

---

## Phase 5: Polish & Cross-Cutting Concerns

**Purpose**: Validation and cleanup

- [X] T010 Validate all new tests pass by running each binary individually: `./tests/block_propagation_tests`, `./tests/peer_manager_tests`
- [X] T011 Validate all existing tests still pass by running each binary individually: `./tests/lifecycle_tests`, `./tests/sync_tests`, `./tests/consensus_tests`
- [X] T012 Update docs/ROADMAP.md to reflect completed test coverage gap
- [X] T013 Update docs/AUDIT.md §7.5 to mark "Peer disconnect during propagation" gap as resolved (satisfies SC-001: 0 remaining open items)
- [X] T014 Run each new test binary 10 times consecutively to verify no flakes (satisfies SC-002): `for i in $(seq 1 10); do ./tests/block_propagation_tests "[relay_exception]" && ./tests/peer_manager_tests "[disconnect]"; done`

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: No blocking tasks for this feature — existing infrastructure is sufficient
- **User Story 1 (Phase 3)**: Depends on Setup — relay exception tests in block_propagation_tests.cpp, disconnect tests in peer_manager_tests.cpp
- **User Story 2 (Phase 4)**: Depends on Setup — inbound disconnect tests in peer_manager_tests.cpp
- **Polish (Phase 5)**: Depends on all user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start immediately after Setup — no dependencies on other stories
- **User Story 2 (P2)**: Can start immediately after Setup — no dependencies on US1 (same file, different test cases)

### Within Each User Story

- All test tasks within a user story are independent (different TEST_CASE blocks)
- Tasks in different files (block_propagation_tests.cpp vs peer_manager_tests.cpp) can run in parallel

### Parallel Opportunities

- T002, T003, T004 (block_propagation_tests.cpp): Sequential within same file but independent test cases
- T005, T006, T007, T008, T009 (peer_manager_tests.cpp): Sequential within same file but independent test cases
- T002-T004 (block_propagation) can run in parallel with T005-T009 (peer_manager) — different files

---

## Parallel Example

```bash
# Terminal 1: Block propagation relay exception tests
# Tasks T002, T003, T004 in tests/block_propagation_tests.cpp

# Terminal 2: Peer manager disconnect handler tests
# Tasks T005, T006, T007, T008, T009 in tests/peer_manager_tests.cpp
```

---

## Implementation Strategy

### MVP Scope (User Story 1 only)
- Relay callback exception handling tests (T002, T003, T004)
- Outbound disconnect handler tests (T005, T006, T007)
- Verifies the highest-risk coverage gap: relay failures during propagation

### Incremental Delivery
1. **MVP**: User Story 1 — core relay exception and outbound disconnect coverage
2. **Increment**: User Story 2 — inbound disconnect and dedup coverage
3. **Polish**: Full test suite validation and ROADMAP update
