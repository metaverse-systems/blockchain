# Implementation Plan: Audit Bug & Security Fixes

**Branch**: `018-audit-bug-security-fixes` | **Date**: 2026-04-13 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/018-audit-bug-security-fixes/spec.md`

## Summary

Fix 4 bugs and 2 security vulnerabilities identified in audit sections 2 and 3.
The sync handler must actually append received blocks (§2.1), chain recovery
must load each chunk only once (§2.2), the `getBlockByIndex` resize must use
correct chunk IDs (§2.3), `parsePeerKey()` must validate port range (§2.4),
seed node CLI parsing must handle invalid input gracefully (§3.1), and the
`getBlockByIndex` RPC handler must bounds-check the requested index (§3.2).

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`)
**Storage**: Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`)
**Testing**: Catch2
**Target Platform**: Linux, macOS, Windows
**Project Type**: CLI / library (blockchain node)
**Performance Goals**: Single-pass chain recovery (N chunks → N loads, not 3N)
**Constraints**: Must not change the existing P2P wire format (`SyncResponse`)
**Scale/Scope**: 6 source files modified, 6 targeted test additions

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Status | Notes |
|---|-----------|--------|-------|
| I | C++20 | PASS | All changes are standard C++20 |
| II | Build System (Autotools, `make -j8`) | PASS | No build system changes |
| III | Full Test Coverage (Catch2, individual binaries) | PASS | Each fix gets targeted tests |
| IV | Code Style | PASS | Follows existing conventions |
| V | Minimal Dependencies | PASS | No new dependencies |
| VI | Mandatory TLS | PASS | No TLS changes |
| VII | Cross-Platform | PASS | No platform-specific code |
| VIII | Feature Branches | PASS | On branch `018-audit-bug-security-fixes` |
| IX | Pre-1.0 API Stability | PASS | No public API changes |
| X | Low-Latency Performance | PASS | Recovery optimization improves startup |
| XI | MIT License | PASS | No third-party code |
| XII | .gitignore | PASS | No new binaries (tests added to existing binaries) |
| XIII | Roadmap Currency | REQUIRED | Must update ROADMAP.md on completion |

**Gate result**: PASS — no violations.

## Project Structure

### Documentation (this feature)

```text
specs/018-audit-bug-security-fixes/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (files modified)

```text
src/
├── network/
│   ├── PeerClient.cpp       # §2.1 — fix handle_sync_response() to append blocks
│   └── RpcServer.cpp        # §3.2 — bounds check getBlockByIndex RPC
├── ChainPersistence.hpp     # §2.2 — change validateChunk() return type
├── ChainPersistence.cpp     # §2.2 — single-pass recovery
├── Blockchain.cpp           # §2.3 — fix resize chunk ID
├── utils.cpp                # §2.4 — port range validation in parsePeerKey()
└── main.cpp                 # §3.1 — safe seed node port parsing

tests/
├── sync_tests.cpp           # Tests for §2.1 (sync appends), §2.2 (recovery)
├── rpc_integration_tests.cpp # Test for §3.2 (getBlockByIndex bounds)
├── blockchain_tests.cpp     # Test for §2.3 (resize IDs)
└── utils_tests.cpp          # Tests for §2.4 (parsePeerKey port range)
```

**Structure Decision**: No new files. All changes are modifications to existing
source and test files.

## Complexity Tracking

No constitution violations — section not applicable.
