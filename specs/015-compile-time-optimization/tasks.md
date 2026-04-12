# Tasks: Compile-Time Optimization

**Input**: Design documents from `/specs/015-compile-time-optimization/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, quickstart.md

**Tests**: Not requested — no test tasks generated.

**Organization**: Tasks are grouped by user story. US1 is the MVP; US2 and US3 depend on US1 completion.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup

**Purpose**: Baseline measurement and preparation

- [ ] T001 Record baseline clean build time with `make clean && time make -j8 check TESTS=`
- [ ] T002 Count baseline compilation units with `make clean && make -j8 check TESTS= 2>&1 | grep -c '\.cpp'`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: No foundational work is needed — all changes are within the user story phases. The existing project structure, Autotools toolchain, and `configure.ac` (`subdir-objects` already enabled) require no pre-work.

**⚠️ CRITICAL**: Phase 2 is intentionally empty. Proceed directly to Phase 3.

---

## Phase 3: User Story 1 - Developer Builds Full Project Faster (Priority: P1) 🎯 MVP

**Goal**: Eliminate redundant compilation by introducing a shared static archive for the 11 core source files, reducing compilation units from ~177 to ~34.

**Independent Test**: Run `make clean && time make -j8 check TESTS=` and verify at least 50% reduction from 13:20 baseline.

### Implementation for User Story 1

- [ ] T003 [US1] Add `noinst_LIBRARIES = libblockchain_core.a` with 11 core sources and flags `-std=c++20 -Wall -Wextra -pedantic $(BOOST_CPPFLAGS) ${OPENSSL_CFLAGS}` in src/Makefile.am
- [ ] T004 [US1] Refactor `blockchain` target in src/Makefile.am to link `libblockchain_core.a` instead of listing core sources directly (keep only main.cpp and CliParser.cpp in `blockchain_SOURCES`)
- [ ] T005 [US1] Refactor `blockchain_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T006 [P] [US1] Refactor `block_propagation_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T007 [P] [US1] Refactor `block_propagation_integration_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T008 [P] [US1] Refactor `chunk_persistence_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T009 [P] [US1] Refactor `chunk_recovery_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T010 [P] [US1] Refactor `chunk_replace_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T011 [P] [US1] Refactor `merkle_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T012 [P] [US1] Refactor `merkle_rpc_integration_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T013 [P] [US1] Refactor `cli_tests` target in tests/Makefile.am to drop all `../src/*.cpp` except `../src/CliParser.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T014 [P] [US1] Refactor `rpc_expansion_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T015 [P] [US1] Refactor `lifecycle_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T016 [P] [US1] Refactor `lifecycle_integration_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T017 [P] [US1] Refactor `rpc_integration_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T018 [P] [US1] Refactor `p2p_sync_integration_tests` target in tests/Makefile.am to drop all `../src/*.cpp` from `_SOURCES` and add `../src/libblockchain_core.a` to `_LDADD`
- [ ] T019 [US1] Run `autoreconf -fi && ./configure` to regenerate Makefiles with the new archive target
- [ ] T020 [US1] Run `make -j8 check TESTS=` and verify all 14 binaries build successfully
- [ ] T021 [US1] Run each test binary individually to verify all tests pass identically
- [ ] T022 [US1] Record new clean build time with `make clean && time make -j8 check TESTS=` and verify ≥50% reduction

**Checkpoint**: Full build completes faster; all tests pass. US1 is the MVP.

---

## Phase 4: User Story 2 - Developer Rebuilds After Single-File Change Faster (Priority: P2)

**Goal**: Verify that incremental builds after a single-file change only recompile the minimum necessary.

**Independent Test**: Touch a single core `.cpp` file and verify only one recompilation + re-archive + re-link occurs.

### Implementation for User Story 2

- [ ] T023 [US2] After a full build, run `touch src/Block.cpp && time make -j8 check TESTS=` and verify only Block.cpp is recompiled, the archive is updated, and affected binaries are re-linked
- [ ] T024 [US2] After a full build, run `touch tests/block_tests.cpp && time make -j8 check TESTS=` and verify only `blockchain_tests` is rebuilt (archive untouched)
- [ ] T025 [US2] After a full build, run `touch src/Block.hpp && time make -j8 check TESTS=` and verify header-change triggers recompilation of dependent archive objects and re-link of all binaries

**Checkpoint**: Incremental builds rebuild only the changed file and its downstream dependents.

---

## Phase 5: User Story 3 - CI Pipeline Builds Complete Faster (Priority: P3)

**Goal**: Ensure CI configuration is compatible with the new build structure and benefits from the optimization.

**Independent Test**: Verify CI workflow YAML still references correct build commands and the optimization applies on all three platforms.

### Implementation for User Story 3

- [ ] T026 [US3] Review .github/workflows/ci.yml and verify no changes are needed; specifically confirm that `make -j8 -C tests check TESTS=` correctly resolves the cross-directory dependency on `../src/libblockchain_core.a`

**Checkpoint**: CI pipeline requires no YAML changes; optimization is inherited automatically.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Housekeeping required by the constitution.

- [ ] T027 [P] Add `src/libblockchain_core.a` to .gitignore
- [ ] T028 Update docs/ROADMAP.md to mark 015-compile-time-optimization as completed
- [ ] T029 Run quickstart.md validation: verify the "How to Add a New Test Binary" example in specs/015-compile-time-optimization/quickstart.md is accurate

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Empty — skip
- **User Story 1 (Phase 3)**: Depends on Phase 1 baseline measurements
  - T003 and T004 must complete before T005–T018 (archive must exist before test targets can reference it)
  - T005–T018 are all parallelizable (independent test target refactors)
  - T019 depends on all T003–T018 being complete
  - T020–T022 are sequential validation steps after T019
- **User Story 2 (Phase 4)**: Depends on US1 (Phase 3) completion — needs working build
- **User Story 3 (Phase 5)**: Depends on US1 (Phase 3) completion — needs working build; independent of US2
- **Polish (Phase 6)**: Depends on all user stories being complete

### Parallel Opportunities

Within Phase 3 (US1):
- T006–T018 can all run in parallel (each modifies a different target block within tests/Makefile.am)
- T027 can run in parallel with T028 and T029

---

## Parallel Example: User Story 1

```
# After T003 and T004 complete, launch all test target refactors in parallel:
T005: blockchain_tests refactor in tests/Makefile.am
T006: block_propagation_tests refactor in tests/Makefile.am
T007: block_propagation_integration_tests refactor in tests/Makefile.am
T008: chunk_persistence_tests refactor in tests/Makefile.am
T009: chunk_recovery_tests refactor in tests/Makefile.am
T010: chunk_replace_tests refactor in tests/Makefile.am
T011: merkle_tests refactor in tests/Makefile.am
T012: merkle_rpc_integration_tests refactor in tests/Makefile.am
T013: cli_tests refactor in tests/Makefile.am
T014: rpc_expansion_tests refactor in tests/Makefile.am
T015: lifecycle_tests refactor in tests/Makefile.am
T016: lifecycle_integration_tests refactor in tests/Makefile.am
T017: rpc_integration_tests refactor in tests/Makefile.am
T018: p2p_sync_integration_tests refactor in tests/Makefile.am
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Baseline measurements (T001–T002)
2. Complete Phase 3: Build restructuring (T003–T022)
3. **STOP and VALIDATE**: Verify ≥50% build time reduction and all tests pass
4. Proceed to Phase 4 (US2) and Phase 5 (US3) for incremental/CI validation
5. Complete Phase 6: Polish (.gitignore, ROADMAP.md)
