# Implementation Plan: Peer Disconnect Test Coverage

**Branch**: `022-test-coverage-gaps` | **Date**: 2026-06-23 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/022-test-coverage-gaps/spec.md`

## Summary

Add unit tests for the last remaining medium-severity coverage gap identified in AUDIT.md §7.5: peer disconnect during block propagation. Tests exercise `PeerManager::on_peer_disconnected()`, `PeerManager::on_inbound_disconnected()`, and relay callback exception handling in `BlockPropagation` — using mock relay callbacks and direct method invocation (no network required). No production code changes are expected.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)  
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored), Catch2 (test only)  
**Storage**: N/A (test-only feature; no persistence changes)  
**Testing**: Catch2 (`block_propagation_tests.cpp`, `peer_manager_tests.cpp`)  
**Target Platform**: Linux, macOS, Windows (cross-platform per constitution §VII)
**Project Type**: CLI library (C++ blockchain library)  
**Performance Goals**: N/A (test coverage only; no performance changes)  
**Constraints**: Tests must be deterministic (no sleep/timing dependencies), must use mock objects per constitution §III. **Principle III waiver**: Network integration tests are not required for this feature; research.md justifies that relay callback exceptions and disconnect state transitions are fully verifiable via unit tests with mock objects (no network required).  
**Scale/Scope**: 9 new test cases across 2 existing test files (3 relay exception + 6 disconnect handler), ~200-300 lines of test code

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard (C++20) | ✅ PASS | Tests use C++20 patterns (structured bindings, std::unordered_map::contains) |
| II. Build System (Autotools, `-j8`) | ✅ PASS | New tests added to existing test binaries via `Makefile.am` (no changes needed) |
| III. Full Test Coverage | ✅ PASS | This feature _adds_ test coverage; uses `MockBlockchain`, `MockChunk` per constitution |
| IV. Code Style | ✅ PASS | Following existing conventions in `block_propagation_tests.cpp` and `peer_manager_tests.cpp` |
| V. Minimal Dependencies | ✅ PASS | No new dependencies |
| VI. Mandatory TLS | ✅ PASS | Tests use mock objects — no network interfaces affected |
| VII. Cross-Platform | ✅ PASS | Tests are pure unit tests (no platform-specific code) |
| VIII. Feature Branches | ✅ PASS | Working on `022-test-coverage-gaps` branch |
| IX. Pre-1.0 API Stability | ✅ PASS | No API changes |
| X. Low-Latency Performance | ✅ PASS | No changes to hot paths |
| XI. MIT License | ✅ PASS | Test files inherit project MIT license |
| XII. .gitignore Maintenance | ✅ PASS | No new build artifacts or binaries |
| XIII. Roadmap Currency | ✅ PASS | Will update `docs/ROADMAP.md` after completion |

## Project Structure

### Documentation (this feature)

```text
specs/022-test-coverage-gaps/
├── plan.md              # This file
├── research.md          # Phase 0 research decisions
├── data-model.md        # Phase 1 data model
├── quickstart.md        # Phase 1 quickstart guide
└── tasks.md             # Phase 2 (created by /speckit.tasks)
```

### Source Code (repository root)

```text
tests/
├── block_propagation_tests.cpp   # Add relay exception tests (~4 new tests)
├── peer_manager_tests.cpp        # Add disconnect handler tests (~5 new tests)
├── MockBlockchain.hpp            # (existing, used by new tests)
└── TestHelpers.hpp               # (existing, used by new tests)
```

**Structure Decision**: Tests are added to two existing test binaries (`block_propagation_tests` and `peer_manager_tests`). No new files, no new build targets. This follows the existing pattern where each test file corresponds to a production module.

## Complexity Tracking

No constitution violations — this is a straightforward test-coverage addition. No exceptions required (Principle III integration test waiver is documented in Technical Context above).
