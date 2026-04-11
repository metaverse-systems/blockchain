# Tasks: CI/CD Pipeline

**Input**: Design documents from `/specs/013-ci-cd-pipeline/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/workflow-schema.md

**Tests**: No test tasks — this feature is CI configuration only, verified by pushing and observing GitHub Actions results.

**Organization**: Tasks grouped by user story. Since this feature produces a single file (`.github/workflows/ci.yml`), most tasks build on each other incrementally within that file.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup

**Purpose**: Create the workflow file with skeleton structure and triggers

- [ ] T001 Create workflow directory and skeleton file at .github/workflows/ci.yml with `name: CI`, trigger config (`on: push: branches: [main]` and `on: pull_request`), and empty `jobs:` section
- [ ] T002 Define build matrix in .github/workflows/ci.yml with `strategy.fail-fast: false` and four `include` entries, each with `name`, `os`, `cc`, `cxx`, and `shell` fields: Linux GCC (name: "Linux GCC", ubuntu-latest, gcc-14, g++-14, bash), Linux Clang (name: "Linux Clang", ubuntu-latest, clang-18, clang++-18, bash), macOS Clang (name: "macOS Clang", macos-latest, clang, clang++, bash), Windows MinGW-w64 (name: "Windows MinGW-w64", windows-latest, gcc, g++, msys2 {0})
- [ ] T003 Add `runs-on: ${{ matrix.os }}` and set `env: CC: ${{ matrix.cc }}, CXX: ${{ matrix.cxx }}` in the job definition in .github/workflows/ci.yml
- [ ] T004 Add `timeout-minutes: 60` to the job definition in .github/workflows/ci.yml and add `actions/checkout@v4` as the first step

---

## Phase 2: Foundational (Dependency Installation)

**Purpose**: Platform-specific dependency installation steps — MUST be complete before build/test steps work

**⚠️ CRITICAL**: Build and test steps depend on all dependencies being installed correctly

- [ ] T005 [US1] Add Linux dependency installation step in .github/workflows/ci.yml: conditional on `runner.os == 'Linux'`, run `sudo apt-get update && sudo apt-get install -y libboost-serialization-dev libboost-program-options-dev libssl-dev catch2`
- [ ] T006 [US1] Add macOS dependency installation step in .github/workflows/ci.yml: conditional on `runner.os == 'macOS'`, run `brew install boost openssl@3 catch2 autoconf automake libtool pkg-config` and export `PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig:$PKG_CONFIG_PATH"`
- [ ] T007 [US1] Add Windows MSYS2 setup step in .github/workflows/ci.yml: use `msys2/setup-msys2@v2` with `msystem: UCRT64`, `update: true`, and install packages `mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-boost mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-catch2 autotools make pkg-config`
- [ ] T008 [US1] Add a `shell` field to each matrix include entry in .github/workflows/ci.yml: `shell: msys2 {0}` for Windows, `shell: bash` for Linux and macOS. Reference `${{ matrix.shell }}` in each `run` step's `shell:` key so that Windows steps use MSYS2 and others use bash

**Checkpoint**: All three platforms can install dependencies. Ready for build steps.

---

## Phase 3: User Story 1 — Automated Build Verification on Push (Priority: P1) 🎯 MVP

**Goal**: Push or PR triggers builds on all three platforms with pass/fail reporting.

**Independent Test**: Push a commit, verify 4 matrix jobs run and produce build results.

### Implementation for User Story 1

- [ ] T009 [US1] Add configure step in .github/workflows/ci.yml: run `autoreconf -fi && ./configure` (macOS must export PKG_CONFIG_PATH before configure)
- [ ] T010 [US1] Add build step in .github/workflows/ci.yml: run `make -j8` (constitution mandates `-j8`)
- [ ] T011 [US1] Set job name to `build (${{ matrix.name }})` in .github/workflows/ci.yml so each matrix entry shows its platform name in the GitHub Actions UI (the `name` field was already defined in T002)

**Checkpoint**: US1 complete — pushing to `main` or opening a PR triggers cross-platform builds with clear per-platform pass/fail.

---

## Phase 4: User Story 2 — Automated Test Execution (Priority: P2)

**Goal**: After successful build, each of 14 test binaries runs as a separate CI step.

**Independent Test**: Push a commit, verify each test binary appears as its own step with individual pass/fail.

### Implementation for User Story 2

- [ ] T012 [US2] Add 14 separate test steps in .github/workflows/ci.yml, each with `timeout-minutes: 5`, running one test binary: `./tests/blockchain_tests`, `./tests/block_propagation_tests`, `./tests/block_propagation_integration_tests`, `./tests/chunk_persistence_tests`, `./tests/chunk_recovery_tests`, `./tests/chunk_replace_tests`, `./tests/merkle_tests`, `./tests/merkle_rpc_integration_tests`, `./tests/cli_tests`, `./tests/rpc_expansion_tests`, `./tests/lifecycle_tests`, `./tests/lifecycle_integration_tests`, `./tests/rpc_integration_tests`, `./tests/p2p_sync_integration_tests`
- [ ] T013 [US2] Name each test step descriptively in .github/workflows/ci.yml (e.g., `name: "Test: blockchain_tests"`, `name: "Test: lifecycle_integration_tests"`) so they appear clearly in the GitHub Actions UI

**Checkpoint**: US2 complete — all 14 test binaries run as separate steps with granular pass/fail reporting per binary per platform.

---

## Phase 5: User Story 3 — Windows MSYS2 Caching (Priority: P3)

**Goal**: MSYS2 packages are cached on Windows; subsequent builds are faster. Linux/macOS caching deferred per research R4 (fast package install, fragile invalidation).

**Independent Test**: Run two consecutive CI builds on Windows and verify the second run restores MSYS2 cache.

### Implementation for User Story 3

- [ ] T014 [US3] Enable MSYS2 caching in .github/workflows/ci.yml: ensure `msys2/setup-msys2@v2` uses `cache: true` (default) and set `release: false` to reuse the pre-installed MSYS2 for faster setup

**Checkpoint**: US3 complete — Windows builds use cached MSYS2 packages on repeat runs.

---

## Phase 6: User Story 4 — Compiler Matrix Coverage (Priority: P4)

**Goal**: Four distinct compiler configurations (Linux GCC, Linux Clang, macOS Clang, Windows MinGW-w64 GCC) all build and test successfully.

**Independent Test**: Push a commit and verify 4 distinct jobs run in the GitHub Actions UI, each labeled with its compiler.

### Implementation for User Story 4

- [ ] T015 [US4] Validate the full matrix in .github/workflows/ci.yml: verify all four matrix entries produce distinct named jobs; ensure Linux Clang and Linux GCC produce separate job runs on ubuntu-latest with different CC/CXX values
- [ ] T016 [US4] Verify each matrix include entry has a `name` field (defined in T002) and that the job name `build (${{ matrix.name }})` (set in T011) renders correctly for all four configurations

**Checkpoint**: US4 complete — 4 compiler-platform configurations run as visually distinct jobs.

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Final validation and documentation

- [ ] T017 Review .github/workflows/ci.yml for YAML syntax correctness: validate with `yamllint` or equivalent, ensure no tabs, correct indentation
- [ ] T018 Run quickstart.md validation: push the workflow to a branch, open a test PR, and verify all acceptance scenarios from spec.md (triggers, status checks, per-binary test steps, matrix labels)
- [ ] T019 Configure GitHub branch protection rules on `main` to require the CI workflow status check to pass before merging (manual step via GitHub repository Settings > Branches > Branch protection rules, or via GitHub API)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 — installs deps needed for build
- **Phase 3 (US1 — Build)**: Depends on Phase 2 — configure and make need deps installed
- **Phase 4 (US2 — Tests)**: Depends on Phase 3 — test binaries need a successful build
- **Phase 5 (US3 — Caching)**: Can run in parallel with Phase 3 (only affects MSYS2 setup step)
- **Phase 6 (US4 — Matrix)**: Can run in parallel with Phase 3 (matrix definition already exists from Phase 1)
- **Phase 7 (Polish)**: Depends on all previous phases

### User Story Dependencies

- **US1 (Build)**: Depends on Foundational (Phase 2) — no dependency on other stories
- **US2 (Tests)**: Depends on US1 — test steps require build to succeed first
- **US3 (Caching)**: Independent of US1/US2 — only touches MSYS2 setup step
- **US4 (Matrix)**: Independent — matrix definition exists from setup; validation can happen after US1

### Parallel Opportunities

- T005, T006, T007 are independent platform-specific install steps (but all live in the same file, so implement sequentially)
- T014 (US3 caching) is independent of T012-T013 (US2 tests)
- T015-T016 (US4 matrix) is independent of T012-T013 (US2 tests)

---

## Parallel Example: After Phase 2

```
After Foundational phase:
  Stream A: T009 → T010 → T011 (US1: Build)
  Stream B: T014 (US3: Caching — independent, touches MSYS2 setup only)
  
After US1 complete:
  Stream A: T012 → T013 (US2: Test steps)
  Stream B: T015 → T016 (US4: Matrix validation)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T004)
2. Complete Phase 2: Foundational (T005–T008)
3. Complete Phase 3: User Story 1 (T009–T011)
4. **STOP and VALIDATE**: Push to a branch, open a PR, verify builds run on all 3 platforms
5. This alone delivers the core CI value

### Incremental Delivery

1. Setup + Foundational → workflow skeleton with deps
2. Add US1 (Build) → cross-platform builds working → **MVP!**
3. Add US2 (Tests) → per-binary test steps visible in UI
4. Add US3 (Caching) → faster Windows repeat builds
5. Add US4 (Matrix validation) → named compiler jobs confirmed
6. Polish → YAML lint, quickstart validation, branch protection rules
