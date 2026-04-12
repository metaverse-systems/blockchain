# Implementation Plan: Code Audit Remediation

**Branch**: `016-audit-remediation` | **Date**: 2026-04-12 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/016-audit-remediation/spec.md`

## Summary

Remediate 5 bugs, 6 duplication clusters, 4 performance issues, and thread-safety gaps identified in the code audit (docs/AUDIT.md). The approach is targeted fixes with shared utilities rather than architectural refactoring: fix `getChainBlockCount()` to use cached total, enforce single-threaded `io_context` with a runtime assertion, add difficulty caching per adjustment boundary, retain chunks during multi-access operations, extract shared helpers for chunk filenames/RPC responses/peer sending/test setup, fix IPv6 sender-key parsing, add verify-then-cache for received merkle roots, replace pending-pool linear scan with `unordered_map` + `deque`, and fix the premature `dirty_` flag clear.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`)
**Storage**: Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`), keys (`keys.dat`), streams (`streams.dat`, `stream_index.dat`)
**Testing**: Catch2
**Target Platform**: Linux, macOS, Windows (cross-platform per constitution §VII)
**Project Type**: CLI / daemon (blockchain node)
**Performance Goals**: Low-latency query/response per constitution §X. Difficulty calculation must not scale with chain length.
**Constraints**: Must use GNU Autotools (constitution §II). Must use `make -j8`. Dependencies limited to approved set (constitution §V).
**Scale/Scope**: ~5,900 lines in `src/`, ~6,750 lines in `tests/`. 13 functional requirements, 6 user stories.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard | PASS | All changes are C++20 compatible |
| II. Build System | PASS | GNU Autotools unchanged. All `make` with `-j8` |
| III. Full Test Coverage | PASS | New tests for each bug fix. Catch2 + mocks. Individual binary execution |
| IV. Code Style | PASS | Follow existing codebase conventions |
| V. Minimal Dependencies | PASS | No new dependencies. All within approved set |
| VI. Mandatory TLS | PASS | No TLS changes |
| VII. Cross-Platform | PASS | No platform-specific code introduced |
| VIII. Feature Branches | PASS | On `016-audit-remediation` branch |
| IX. Pre-1.0 API Stability | PASS | Internal refactoring only. No protocol changes |
| X. Low-Latency Performance | PASS | Performance improvements (difficulty cache, chunk retention, O(1) eviction) |
| XI. MIT License | PASS | No new third-party code |
| XII. .gitignore | PASS | No new build targets |
| XIII. Roadmap Currency | PASS | Will update ROADMAP.md on completion |

**Gate result**: ALL PASS — proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/016-audit-remediation/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (N/A — internal refactoring)
└── tasks.md             # Phase 2 output (not created by /speckit.plan)
```

### Source Code (repository root)

```text
src/
├── Block.cpp            # FR-009: verify-then-cache merkle root
├── Block.hpp            # FR-009: new constructor overload
├── Blockchain.cpp       # FR-001, FR-003, FR-004, FR-011, FR-012: block count, difficulty cache, chunk retention, dirty flag, stream entries
├── Blockchain.hpp       # FR-003: difficulty cache member
├── BlockPropagation.cpp # FR-008, FR-013: IPv6 parsing, O(1) pending pool
├── BlockPropagation.hpp # FR-013: data structure change
├── PeerManager.cpp      # FR-007: unified send_to_peers()
├── main.cpp             # FR-002: single-thread assertion + documentation
├── utils.cpp            # FR-005, FR-008: chunkFilename(), parsePeerKey()
├── utils.hpp            # FR-005, FR-008: utility declarations
├── network/
│   └── RpcServer.cpp    # FR-006: shared makeErrorResponse()

tests/
├── TestHelpers.hpp      # FR-010: shared test utilities (NEW)
├── block_tests.cpp      # Updated to use TestHelpers
├── consensus_tests.cpp  # Updated to use TestHelpers
├── chunk_persistence_tests.cpp  # Updated to use TestHelpers
├── lifecycle_tests.cpp  # Updated to use TestHelpers
└── ... (all test files updated for shared helpers)
```

**Structure Decision**: All changes are in existing `src/` and `tests/` directories. One new file: `tests/TestHelpers.hpp`. No structural changes to project layout.

## Complexity Tracking

No constitution violations to justify.
