# Tasks: CLI & Configuration

**Input**: Design documents from `/specs/009-cli-configuration/`
**Prerequisites**: plan.md (required), spec.md (required for user stories), research.md, data-model.md, contracts/

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Build system changes and new source file scaffolding

- [ ] T001 Add `AX_BOOST_PROGRAM_OPTIONS` check to `configure.ac` and regenerate configure
- [ ] T002 Add `-lboost_program_options` to `blockchain_LDADD` in `src/Makefile.am`
- [ ] T003 [P] Create `src/CliParser.hpp` with `CliOptions` struct and `CliParser` class declaration per data-model.md
- [ ] T004 [P] Create `src/CliParser.cpp` with stub `parse()` that returns default `CliOptions`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Log-level enum and filtering infrastructure that all user stories depend on

**CRITICAL**: No user story work can begin until this phase is complete

- [ ] T005 Add `LogLevel` enum, `setLogLevel()`, `getLogLevel()`, and `parseLogLevel()` declarations to `src/utils.hpp`
- [ ] T006 Implement `LogLevel` global atomic, `setLogLevel()`, `getLogLevel()`, `parseLogLevel()` in `src/utils.cpp`; add early-return filter and `"DEBUG"` color to `logMessage()`
- [ ] T007 Add `log_level` field (`std::string`, default `"info"`) to `NodeConfig::NetworkConfig` in `src/NodeConfig.hpp`
- [ ] T008 Add `log_level` parsing to `NodeConfig::load()` and add `"log_level"` to `default_json()` network section in `src/NodeConfig.cpp`

**Checkpoint**: Foundation ready — LogLevel filtering works, NodeConfig reads log_level from config.json

---

## Phase 3: User Story 1 — Display Help and Usage Information (Priority: P1) MVP

**Goal**: Operators can run `--help` and `--version` to discover CLI options and version info

**Independent Test**: Run `blockchain --help` and `blockchain --version`; verify structured output and clean exit code 0

### Implementation for User Story 1

- [ ] T009 [US1] Implement `CliParser::parse()` in `src/CliParser.cpp` using Boost.ProgramOptions with all option definitions from contracts/cli-interface.md (--help, --version, --config, --rpc-port, --p2p-port, --seed-node, --log-level, --generate-config, positional blockchain-dir)
- [ ] T010 [US1] Add version output using `PACKAGE_NAME` and `PACKAGE_VERSION` from `config.h` in `src/CliParser.cpp`; include `config.h` via adjusted include path or `-I` flag
- [ ] T011 [US1] Replace manual `argc`/`argv` parsing in `src/main.cpp` with `CliParser::parse()`; handle `--help` (print and exit 0), `--version` (print and exit 0), and no-arguments (print usage and exit 1)
- [ ] T012 [US1] Add CLI parser unit tests in `tests/cli_tests.cpp`: test --help exits cleanly, --version prints version string, no-args shows usage; add `cli_tests` to `check_PROGRAMS` in `tests/Makefile.am`

**Checkpoint**: `blockchain --help` and `blockchain --version` work. No-args shows usage. All new tests pass via `make check`.

---

## Phase 4: User Story 2 — Override Configuration via Command-Line Flags (Priority: P1)

**Goal**: Operators can pass `--rpc-port`, `--p2p-port`, `--seed-node`, `--log-level`, and `--config` flags to override config.json values

**Independent Test**: Start with `--rpc-port 9999 --p2p-port 9998` and verify the daemon binds to those ports

### Implementation for User Story 2

- [ ] T013 [US2] Add CLI override application logic in `src/main.cpp`: after `NodeConfig::load()`, apply `CliOptions.rpc_port`, `p2p_port`, `seed_nodes`, `log_level` onto the loaded `NodeConfig` fields; apply `--config` path resolution before loading
- [ ] T014 [US2] Add blockchain directory existence check in `src/main.cpp` before config loading: if directory does not exist, print error to stderr and exit 1 (FR-016)
- [ ] T015 [US2] Set global log level from final `NodeConfig.network.log_level` via `setLogLevel(parseLogLevel(...))` in `src/main.cpp` after override application
- [ ] T016 [US2] Add CLI override unit tests in `tests/cli_tests.cpp`: test --rpc-port overrides config value, --p2p-port overrides config value, --seed-node appends to config seeds, --config loads alternate path, --log-level sets level, missing blockchain dir exits with error

**Checkpoint**: All CLI flags override config.json correctly. Precedence chain CLI > file > defaults confirmed by tests.

---

## Phase 5: User Story 3 — Set Log Verbosity Level (Priority: P2)

**Goal**: Operators control log output via `--log-level` and `network.log_level` in config.json

**Independent Test**: Start with `--log-level error`, perform operations, verify only error messages appear

### Implementation for User Story 3

- [ ] T017 [US3] Add `logMessage` filtering tests in `tests/cli_tests.cpp`: test that setting LogLevel::Error suppresses INFO/WARN messages, LogLevel::Debug shows all, LogLevel::Info suppresses DEBUG; test invalid log level string returns error
- [ ] T018 [US3] Add config.json `log_level` round-trip test in `tests/node_config_tests.cpp`: write config with `"log_level": "debug"`, load it, verify `network.log_level == "debug"`; verify default is `"info"` when key is missing

**Checkpoint**: Log filtering works at all four levels. Config.json log_level is read and applied correctly.

---

## Phase 6: User Story 4 — Generate Default Configuration File (Priority: P2)

**Goal**: Operators can run `--generate-config` to create a default config.json and companion config.README

**Independent Test**: Run `blockchain --generate-config /tmp/test-dir`, verify both files created, inspect contents

### Implementation for User Story 4

- [ ] T019 [US4] Implement `config.README` generation in `src/NodeConfig.cpp`: add a `generate_readme()` static method that writes a text file documenting each config section, field name, type, valid values, and default
- [ ] T020 [US4] Implement `--generate-config` handler in `src/main.cpp`: after parsing CLI, if `generate_config` is true, verify blockchain_dir exists, check for existing config.json (refuse if exists, exit 1), call `NodeConfig::generate_default()` and `NodeConfig::generate_readme()`, exit 0
- [ ] T021 [US4] Add `--generate-config` tests in `tests/cli_tests.cpp`: test config.json and config.README are created in empty directory; test existing config.json is not overwritten (exit 1); test generated config.json is valid and loadable

**Checkpoint**: `--generate-config` creates both files. Refusal to overwrite confirmed by tests.

---

## Phase 7: User Story 5 — Validate Configuration at Startup (Priority: P3)

**Goal**: All config errors are caught and reported at startup with clear messages before any network binding

**Independent Test**: Start with invalid port in config.json, verify all errors listed and daemon exits

### Implementation for User Story 5

- [ ] T022 [US5] Enhance `NodeConfig::validate()` in `src/NodeConfig.cpp` to accumulate all errors in a `std::vector<std::string>` instead of throwing on the first; add port range check (1–65535), port conflict check (rpc != p2p), TLS cert/key file existence check (resolved relative to blockchain dir); throw single exception with all errors joined
- [ ] T023 [US5] Add `validate()` method signature update in `src/NodeConfig.hpp`: accept `std::filesystem::path blockchain_dir` parameter so TLS paths can be resolved
- [ ] T024 [US5] Add unknown key detection in `NodeConfig::load()` in `src/NodeConfig.cpp`: after parsing, iterate JSON keys at top-level and per-section, compare against known key registry from data-model.md, call `logMessage("WARN", ...)` for each unknown key
- [ ] T025 [US5] Update validation call site in `src/main.cpp` to pass `blockchainDir` to `validate()`; wrap in try/catch to print all errors to stderr before exiting
- [ ] T026 [US5] Add validation tests in `tests/node_config_tests.cpp`: test port out of range triggers error, equal ports trigger error, missing TLS cert file triggers error, multiple errors collected in single throw, unknown config key warns but does not fail, valid config passes

**Checkpoint**: Invalid configs produce multi-error output. Unknown keys warn. All tests pass.

---

## Phase 8: Polish & Cross-Cutting Concerns

**Purpose**: Final integration verification

- [ ] T027 [P] Update `tests/Makefile.am` to ensure `cli_tests` links against `boost_program_options` and all required source files
- [ ] T028 Run full `make check` to verify all existing and new tests pass; include one integration test that starts the daemon with `--rpc-port 9999 --p2p-port 9998` and verifies bind via a connection attempt (constitution §III)
- [ ] T029 Run quickstart.md validation steps 1–10 against built binary

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup (T001–T004) — BLOCKS all user stories
- **User Story 1 (Phase 3)**: Depends on Foundational (Phase 2)
- **User Story 2 (Phase 4)**: Depends on User Story 1 (Phase 3) — needs parse() complete
- **User Story 3 (Phase 5)**: Depends on Foundational (Phase 2) — can run parallel to US1/US2
- **User Story 4 (Phase 6)**: Depends on User Story 1 (Phase 3) — T020 requires `CliParser::parse()` from T009; T019 can start after Foundational
- **User Story 5 (Phase 7)**: T022–T024 depend on Foundational (Phase 2) only; T025 depends on User Story 1 (Phase 3) for main.cpp integration
- **Polish (Phase 8)**: Depends on all user stories complete

### User Story Dependencies

- **US1 (P1)**: Independent after Foundational
- **US2 (P1)**: Depends on US1 (needs the full `CliParser::parse()` from T009)
- **US3 (P2)**: Independent after Foundational (only needs LogLevel infrastructure from Phase 2)
- **US4 (P2)**: T019 can start after Foundational; T020–T021 depend on T009 (US1) for `CliParser::parse()` and main.cpp integration
- **US5 (P3)**: T022–T024 can start after Foundational; T025 depends on T009 (US1) for main.cpp integration

### Within Each User Story

- Implementation tasks before tests (no TDD requested in spec)
- Core implementation before integration
- Story complete before moving to next priority

### Parallel Opportunities

- T003 and T004 can run in parallel (different files)
- US3, US4, US5 can all run in parallel once US1 is complete (different files/areas)
- T027 can run in parallel with T028/T029

---

## Parallel Example: After Phase 2

```
# Once Foundational phase is complete, these can proceed in parallel:
Stream A: US1 (T009–T012) → US2 (T013–T016)
                          → US4 T020–T021 (after T009)
                          → US5 T025 (after T009)
Stream B: US3 (T017–T018)
Stream C: US4 T019 (after Phase 2, parallel to US1)
Stream D: US5 T022–T024 (after Phase 2, parallel to US1)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (T001–T004)
2. Complete Phase 2: Foundational (T005–T008)
3. Complete Phase 3: User Story 1 (T009–T012)
4. **STOP and VALIDATE**: Test `--help` and `--version` independently
5. Continue with US2 for full CLI override capability

### Full Delivery

1. Phases 1–4 (Setup → Foundational → US1 → US2): Core CLI functionality
2. Phases 5–7 (US3, US4, US5): Log levels, config generation, validation — can parallelize
3. Phase 8: Polish and final validation
