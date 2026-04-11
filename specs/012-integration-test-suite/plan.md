# Implementation Plan: Integration Test Suite

**Branch**: `012-integration-test-suite` | **Date**: 2026-04-11 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/012-integration-test-suite/spec.md`

## Summary

Add end-to-end integration tests that exercise all JSON-RPC endpoints and two-node P2P sync over real TLS connections. Tests run blockchain nodes in-process on separate threads with self-signed certificates generated at test time, dynamically assigned ports, and RAII-based cleanup.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (EVP SHA-256, X.509 cert generation), nlohmann/json (vendored `src/json.hpp`), Catch2 (test framework)
**Storage**: Boost.Serialization binary chunk files in temporary directories (cleaned per test)
**Testing**: Catch2 — same framework used by all existing test binaries
**Target Platform**: Linux, macOS, Windows (per Constitution §VII)
**Project Type**: C++ blockchain library/daemon with RPC and P2P interfaces
**Performance Goals**: Each integration test completes within 60 seconds; P2P sync of a 10-block chain within 30 seconds (SC-003, SC-006)
**Constraints**: In-process threading model (clarified in spec); no new external dependencies (Constitution §V); TLS mandatory on all network interfaces (Constitution §VI)
**Scale/Scope**: ~2 new test binaries, ~20–25 test cases total

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Status | Notes |
|---|-----------|--------|-------|
| I | C++20 standard | PASS | All new test code uses `-std=c++20` matching existing test flags |
| II | GNU Autotools build system | PASS | New test binaries registered in `tests/Makefile.am`; `make -j8` |
| III | Full test coverage | PASS | This feature IS the integration test suite; tests are runnable individually |
| IV | Code style | PASS | Follow existing conventions (`#pragma once`, naming, braces) |
| V | Minimal dependencies | PASS | No new dependencies; OpenSSL programmatic cert generation uses existing dep |
| VI | Mandatory TLS | PASS | All test connections use real TLS with self-signed certs |
| VII | Cross-platform support | PASS | Boost.Asio port-0 binding + OpenSSL cert API work on all three platforms |
| VIII | Feature branches with PRs | PASS | Already on `012-integration-test-suite` branch |
| IX | Pre-1.0 API stability | N/A | Tests consume existing interfaces, no protocol changes |
| X | Low-latency performance | N/A | Test-only code, no production hot paths |
| XI | MIT license | PASS | New test files under MIT |
| XII | .gitignore maintenance | PASS | New test binaries will be added to `.gitignore` |
| XIII | Roadmap currency | PASS | `docs/ROADMAP.md` will be updated on completion |

**Gate result**: ALL PASS — proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/012-integration-test-suite/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code (repository root)

```text
tests/
├── rpc_integration_tests.cpp       # NEW — full RPC endpoint tests over real TLS
├── p2p_sync_integration_tests.cpp  # NEW — two-node P2P sync and propagation tests
├── IntegrationTestFixture.hpp      # NEW — shared node lifecycle, TLS cert gen, port allocation
├── Makefile.am                     # MODIFIED — add two new test binaries
src/
├── (no source modifications expected)
.gitignore                           # MODIFIED — add new test binary names
```

**Structure Decision**: New test files live alongside existing tests in `tests/`. A shared header (`IntegrationTestFixture.hpp`) provides the in-process node fixture reused by both test binaries. This mirrors the existing pattern where `MockBlockchain.hpp` is shared across test binaries.
