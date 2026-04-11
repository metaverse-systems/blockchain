# Implementation Plan: CLI & Configuration

**Branch**: `009-cli-configuration` | **Date**: 2026-04-11 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/009-cli-configuration/spec.md`

## Summary

Add a proper CLI argument parser to the blockchain daemon so operators can pass `--help`, `--version`, `--rpc-port`, `--p2p-port`, `--seed-node`, `--log-level`, `--config`, and `--generate-config` flags. Command-line flags override `config.json` values, which override built-in defaults. A log-level filtering system replaces the unconditional `logMessage` output. The `--generate-config` command writes both a default `config.json` and a companion `config.README` describing each field.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`), getopt (POSIX, header-only)
**Storage**: `config.json` and `config.README` in blockchain data directory; Boost.Serialization binary archives for chain data
**Testing**: Catch2 via `make check`
**Target Platform**: Linux, macOS, Windows (constitution §VII)
**Project Type**: CLI daemon
**Performance Goals**: N/A — CLI parsing is startup-only, not on hot path
**Constraints**: No new external dependencies (constitution §V); must use `PACKAGE_VERSION` from Autoconf `config.h`
**Scale/Scope**: Single daemon binary, ~150 LOC new code for CLI parser, ~80 LOC for log-level filtering, ~60 LOC for enhanced validation

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard (C++20) | PASS | All new code uses C++20 features only |
| II. Build System (Autotools) | PASS | No build system changes beyond adding new source files to `Makefile.am` |
| III. Full Test Coverage | PASS | Unit tests for CLI parsing, log filtering, validation; integration tests for `--help`/`--version` exit behavior |
| IV. Code Style | PASS | Follows existing naming/indentation conventions |
| V. Minimal Dependencies | PASS | CLI parsing uses `getopt_long` (POSIX standard, no new dependency). Windows portability addressed via a bundled `getopt` shim or Boost.ProgramOptions fallback — see research.md |
| VI. Mandatory TLS | PASS | No TLS changes |
| VII. Cross-Platform Support | NEEDS RESEARCH | `getopt_long` is POSIX; Windows MSVC does not ship it. Research needed for cross-platform CLI parsing approach |
| VIII. Feature Branches | PASS | Working on `009-cli-configuration` branch |
| IX. Pre-1.0 API Stability | PASS | No protocol changes |
| X. Low-Latency Performance | PASS | CLI parsing is startup-only |
| XI. MIT License | PASS | No new third-party code with incompatible licenses |

## Project Structure

### Documentation (this feature)

```text
specs/009-cli-configuration/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
└── tasks.md             # Phase 2 output (NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
src/
├── main.cpp             # MODIFY — replace manual argc/argv with CLI parser, apply overrides
├── CliParser.hpp        # NEW — CLI argument parsing (getopt_long wrapper)
├── CliParser.cpp        # NEW — CLI argument parsing implementation
├── NodeConfig.hpp       # MODIFY — add log_level to NetworkConfig, add enhanced validation
├── NodeConfig.cpp       # MODIFY — add log_level parsing, unknown key warnings, config.README generation
├── utils.hpp            # MODIFY — add LogLevel enum and setLogLevel/getLogLevel
├── utils.cpp            # MODIFY — add log level filtering to logMessage
└── ...                  # Unchanged

tests/
├── cli_tests.cpp        # NEW — unit tests for CliParser
├── node_config_tests.cpp # MODIFY — add tests for log_level, unknown keys, port conflict, TLS file checks
└── Makefile.am          # MODIFY — add cli_tests to check_PROGRAMS
```

**Structure Decision**: Single project layout (existing). Two new source files (`CliParser.hpp/cpp`) in `src/`, one new test file (`cli_tests.cpp`) in `tests/`. All other changes are modifications to existing files.

## Complexity Tracking

No constitution violations requiring justification.
