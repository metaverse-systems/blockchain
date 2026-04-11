# Implementation Plan: RPC API Expansion

**Branch**: `010-rpc-api-expansion` | **Date**: 2026-04-11 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/010-rpc-api-expansion/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Add four new JSON-RPC methods to the existing RPC server: `getNodeStatus` (comprehensive node health snapshot), `getBlockRange` (batch block retrieval with optional headers-only mode and 1000-block cap), `getChainLength`, and `getChunkCount`. All methods are read-only, follow existing JSON-RPC 2.0 conventions, and require no transport-layer changes. One interface change is needed: adding `getCurrentDifficulty()` to `IBlockchain`.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored `src/json.hpp`)
**Storage**: N/A (read-only endpoints; no new persistence)
**Testing**: Catch2 (unit + integration tests via `make check`)
**Target Platform**: Linux, macOS, Windows (cross-platform per constitution)
**Project Type**: Daemon (blockchain node with JSON-RPC + P2P interfaces)
**Performance Goals**: Low-latency query and response (constitution Principle X); `getBlockRange` capped at 1000 blocks to bound memory and response size
**Constraints**: All methods are read-only, no state modification; must not block on sync operations
**Scale/Scope**: 4 new RPC methods, 1 interface change (`IBlockchain`), 1 source file edit (`RpcServer.cpp`), 1 new test file

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard (C++20) | PASS | All new code uses C++20; no later features |
| II. Build System (GNU Autotools) | PASS | Tests added via `Makefile.am`; no build system changes |
| III. Full Test Coverage | PASS | Unit tests for all 4 methods + error paths; integration tests for getBlockRange |
| IV. Code Style | PASS | Follows existing if-else dispatch pattern and response helpers |
| V. Minimal Dependencies | PASS | No new dependencies |
| VI. Mandatory TLS | PASS | Uses existing TLS-wrapped RPC server; no transport changes |
| VII. Cross-Platform | PASS | No platform-specific code; uses standard C++ and existing abstractions |
| VIII. Feature Branches | PASS | Working on `010-rpc-api-expansion` branch |
| IX. Pre-1.0 API Stability | PASS | New methods only; no breaking changes to existing API |
| X. Low-Latency Performance | PASS | Read-only methods with O(n) bounded by 1000-block cap |
| XI. MIT License | PASS | No new third-party code |
| XII. .gitignore Maintenance | PASS | New test binary must be added to `.gitignore` |
| XIII. Roadmap Currency | PASS | `docs/ROADMAP.md` update required when feature completes |

**Pre-design gate**: All 13 principles PASS. Proceeding to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/010-rpc-api-expansion/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (JSON-RPC contract)
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
src/
├── IBlockchain.hpp      # Add getCurrentDifficulty() virtual method
├── Blockchain.hpp       # Already has getCurrentDifficulty() — add override keyword
├── network/
│   └── RpcServer.cpp    # Add 4 new method handlers in if-else chain

tests/
├── rpc_expansion_tests.cpp  # New: unit + integration tests for all 4 methods
├── Makefile.am              # Add new test binary
```

**Structure Decision**: Single-project layout. All changes are in existing source tree; one new test file.
