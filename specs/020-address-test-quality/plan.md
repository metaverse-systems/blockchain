# Implementation Plan: Address Test Quality

**Branch**: `020-address-test-quality` | **Date**: 2026-04-13 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/020-address-test-quality/spec.md`

## Summary

Comprehensive test-quality remediation covering all 26 test files (~150+ test cases): replace trivial/vacuous assertions with behavioral checks, rewrite `rpc_expansion_tests.cpp` to call extracted handler methods directly, make integration tests deterministic, close 6 open coverage gaps from the audit, narrow `IBlockchain` into reader/writer sub-interfaces for testability, and modify `saveAllChunks()` error reporting to enable partial-failure testing.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`), Catch2 (test framework)
**Storage**: Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`), keys (`keys.dat`), streams (`streams.dat`, `stream_index.dat`)
**Testing**: Catch2 — 26 test files in `tests/`, mock objects (`MockChunk`, `MockSessionHandler`, `MockAcceptor`), shared `TestHelpers` namespace
**Target Platform**: Linux, macOS, Windows (cross-platform)
**Project Type**: Library / daemon (blockchain node)
**Performance Goals**: Full test suite completes within reasonable bound; no individual test >30s (SC-006)
**Constraints**: No new external dependencies (Constitution §V); all handler tests must be unit-style (direct invocation, no socket overhead)
**Scale/Scope**: 26 test files, ~150+ test cases, 20 RPC handlers, 28 `IBlockchain` methods

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard | ✅ PASS | All new code C++20 compliant |
| II. Build System | ✅ PASS | GNU Autotools; all `make` invocations use `-j8` |
| III. Full Test Coverage | ✅ PASS | Feature is specifically about improving test quality; unit + integration tests included |
| IV. Code Style | ✅ PASS | Follow existing codebase conventions |
| V. Minimal Dependencies | ✅ PASS | No new dependencies; Catch2 already approved |
| VI. Mandatory TLS | ✅ PASS | No TLS changes; network tests already use SSL contexts |
| VII. Cross-Platform Support | ✅ PASS | No platform-specific changes expected |
| VIII. Feature Branches | ✅ PASS | Working on `020-address-test-quality` branch |
| IX. Pre-1.0 API Stability | ✅ PASS | Internal API changes (IBlockchain split) are fine pre-1.0 |
| X. Low-Latency Performance | ✅ PASS | No production hot-path changes; interface split is zero-cost (virtual dispatch unchanged) |
| XI. MIT License | ✅ PASS | No new third-party code |
| XII. .gitignore Maintenance | ⚠️ WATCH | If any new test binaries are added, `.gitignore` must be updated |
| XIII. Roadmap Currency | ✅ PASS | Will update `docs/ROADMAP.md` on completion |

**Gate result: PASS** — No violations. XII requires vigilance if new test binaries are introduced.

## Project Structure

### Documentation (this feature)

```text
specs/020-address-test-quality/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
src/
├── IBlockchain.hpp          # Split into IChainReader + IChainWriter
├── IChainReader.hpp         # NEW — read-only interface
├── IChainWriter.hpp         # NEW — mutation interface
├── Blockchain.hpp           # Implements both IChainReader and IChainWriter
├── Blockchain.cpp
├── ChainPersistence.hpp     # Modified saveAllChunks() error reporting
├── network/
│   ├── RpcServer.hpp        # Handlers made testable (friend or public)
│   └── RpcServer.cpp
└── ...

tests/
├── TestHelpers.hpp          # Extended with new shared helpers
├── server_tests.cpp         # Trivial assertions replaced
├── rpc_expansion_tests.cpp  # Rewritten to call real handlers
├── block_propagation_tests.cpp  # Vacuous assertions fixed + new coverage
├── chunk_persistence_tests.cpp  # Timer tests made deterministic + new coverage
├── lifecycle_tests.cpp      # Assertions strengthened
├── consensus_tests.cpp      # Vacuous assertions fixed
├── sync_tests.cpp           # Vacuous assertions fixed
├── p2p_sync_integration_tests.cpp  # Timing-dependency removed
├── rpc_integration_tests.cpp       # Connection readiness verified
└── ...                      # All 26 files audited
```

**Structure Decision**: Existing `src/` + `tests/` layout is preserved. Two new header files (`IChainReader.hpp`, `IChainWriter.hpp`) are added under `src/`. No new test binaries are needed — new test cases are added to existing test files.

## Complexity Tracking

No constitution violations — table not required.
