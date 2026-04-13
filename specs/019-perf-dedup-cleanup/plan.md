# Implementation Plan: Performance & Deduplication Cleanup

**Branch**: `019-perf-dedup-cleanup` | **Date**: 2026-04-13 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/019-perf-dedup-cleanup/spec.md`

## Summary

Resolve four performance issues (O(n) peer lookups, O(n) RPC dispatch, duplicated packet serialization, eager log formatting) and two code duplication clusters (PeerClient/PeerServer send templates, test helpers) identified in the codebase audit. All changes are internal refactors — no protocol changes, no new features, no public API changes.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`), Catch2 (test only)
**Storage**: Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`), JSON files (`peers.json`, `config.json`)
**Testing**: Catch2 (unit + integration), each test binary run individually
**Target Platform**: Linux, macOS, Windows (cross-platform required)
**Project Type**: CLI / daemon — blockchain node with P2P networking and JSON-RPC interface
**Performance Goals**: O(1) peer lookups and RPC dispatch; zero heap allocation for suppressed log calls
**Constraints**: Wire-format compatibility (no P2P protocol break); all 20 existing RPC methods must return identical responses
**Scale/Scope**: Max 256 stored peers, 20 RPC methods, 4 test files with duplicated helpers

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard (C++20) | PASS | All changes use C++20; no later features |
| II. Build System (Autotools, make -j8) | PASS | New header-only file needs no Makefile.am change; no build system migration |
| III. Full Test Coverage (Catch2, individual binaries) | PASS | Existing tests are refactored, not removed; no new test binaries needed |
| IV. Code Style | PASS | Follow existing conventions (naming, indentation, `#pragma once`) |
| V. Minimal Dependencies | PASS | No new dependencies; uses only approved set |
| VI. Mandatory TLS | N/A | No network protocol changes |
| VII. Cross-Platform Support | PASS | `std::unordered_map`, preprocessor-guarded log macro compatible with all three targets |
| VIII. Feature Branches with PRs | PASS | Working on `019-perf-dedup-cleanup` branch |
| IX. Pre-1.0 API Stability | N/A | No API changes |
| X. Low-Latency Performance | PASS | This feature specifically improves hot-path latency |
| XI. MIT License | PASS | No third-party code added |
| XII. .gitignore Maintenance | N/A | No new build targets |
| XIII. Roadmap Currency | PASS | Will update `docs/ROADMAP.md` on completion |

**Gate result**: PASS — no violations.

## Project Structure

### Documentation (this feature)

```text
specs/019-perf-dedup-cleanup/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (RPC contract)
└── tasks.md             # Phase 2 output (speckit.tasks)
```

### Source Code (repository root)

```text
src/
├── PeerManager.hpp          # Modified: vector→unordered_map for peers_; vector→unordered_map for bans_
├── PeerManager.cpp          # Modified: all peer/ban operations updated for map API
├── BlockPropagation.cpp     # Modified: logMessage() calls → LOG_* macros
├── utils.hpp                # Modified: add LOG_MSG macro
├── network/
│   ├── PacketHeader.hpp     # Unchanged
│   ├── PacketSerializer.hpp # NEW: header-only shared send template
│   ├── PeerClient.cpp       # Modified: replace send<T> body with PacketSerializer call
│   ├── PeerServer.cpp       # Modified: replace send_packet<T> body with PacketSerializer call
│   ├── RpcServer.hpp        # Modified: add dispatch table type + registration
│   └── RpcServer.cpp        # Modified: extract handlers, replace if/else with dispatch table

tests/
├── TestHelpers.hpp              # Modified: add make_block() variant; ensure all helper signatures cover all usages
├── sync_tests.cpp               # Modified: remove local mineTestBlock/buildValidChain, use TestHelpers
├── consensus_tests.cpp          # Modified: remove local mineBlock, use TestHelpers
├── block_propagation_tests.cpp  # Modified: remove local helpers, use TestHelpers
└── chunk_persistence_tests.cpp  # Modified: remove local make_block, use TestHelpers

docs/
├── AUDIT.md                     # Modified: mark resolved audit items
└── ROADMAP.md                   # Modified: move feature to Completed (Constitution §XIII)
```

**Structure Decision**: No new directories or binaries. One new header-only file (`PacketSerializer.hpp`) in existing `src/network/`. All other changes are modifications to existing files.

## Complexity Tracking

No constitution violations — this section is empty.
