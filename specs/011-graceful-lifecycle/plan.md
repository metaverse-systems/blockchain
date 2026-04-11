# Implementation Plan: Graceful Multi-Chunk Shutdown & Startup

**Branch**: `011-graceful-lifecycle` | **Date**: 2026-04-11 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/011-graceful-lifecycle/spec.md`

## Summary

The current shutdown handler saves only the active (last) chunk and index files; historical chunks with unsaved modifications are lost. Startup loads only the active chunk into memory, creating placeholders for earlier chunks. This feature fixes the full lifecycle: shutdown freezes block ingestion then iterates all dirty chunks to persist them, startup discovers and validates all chunk files on disk with cross-chunk linkage checks, and a per-chunk dirty flag prevents unnecessary writes during both periodic and shutdown saves. A `fast_startup` config option allows skipping validation for trusted environments.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`)
**Storage**: Boost.Serialization binary archives for chunk files (`chunk_NNNNNN.dat`), keys (`keys.dat`), streams (`streams.dat`, `stream_index.dat`)
**Testing**: Catch2 (unit and integration tests), MockChunk / MockBlockchain for isolation
**Target Platform**: Linux, macOS, Windows (cross-platform per constitution)
**Project Type**: Daemon/library (blockchain node)
**Performance Goals**: Chunk-based architecture (100 blocks/chunk) supports efficient lookup; dirty-tracking minimizes I/O
**Constraints**: Single-threaded Boost.Asio event loop; no new external dependencies
**Scale/Scope**: Unbounded number of chunks (disk-limited); on-demand historical chunk loading keeps memory bounded

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard (C++20) | PASS | All changes use C++20; no later features |
| II. Build System (Autotools) | PASS | Existing Makefile.am targets extended, no new build system |
| III. Full Test Coverage | PASS | Unit tests for dirty tracking, shutdown save, startup recovery; integration tests for multi-chunk lifecycle |
| IV. Code Style | PASS | Follows existing conventions (naming, braces, `#pragma once`) |
| V. Minimal Dependencies | PASS | No new external dependencies; uses existing Boost, OpenSSL |
| VI. Mandatory TLS | N/A | No network interface changes |
| VII. Cross-Platform | PASS | Using `<csignal>`, `<filesystem>`, Boost.Asio — all portable |
| VIII. Feature Branches | PASS | On branch `011-graceful-lifecycle` |
| IX. Pre-1.0 Stability | N/A | No protocol changes |
| X. Low-Latency Performance | PASS | Dirty-tracking reduces I/O; on-demand loading avoids memory bloat |
| XI. MIT License | PASS | No new third-party code |
| XII. .gitignore Maintenance | PASS | New test binaries added to .gitignore |
| XIII. Roadmap Currency | PASS | docs/ROADMAP.md updated on completion |

**Gate result: PASS — no violations.**

## Project Structure

### Documentation (this feature)

```text
specs/011-graceful-lifecycle/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (config.json schema)
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (repository root)

```text
src/
├── Blockchain.hpp       # Add per-chunk dirty tracking; modify saveAllChunks, recoverChain
├── Blockchain.cpp       # Implement dirty-aware save, full startup load, block-freeze shutdown
├── IChunk.hpp           # Add dirty flag to chunk base class
├── NodeConfig.hpp       # Add fast_startup to PersistenceConfig
├── NodeConfig.cpp       # Parse/validate fast_startup from config.json
├── main.cpp             # Update signal handler: freeze blocks → save dirty → stop

tests/
├── lifecycle_tests.cpp  # Unit tests: dirty tracking, shutdown save, startup recovery
├── lifecycle_integration_tests.cpp  # Integration: multi-chunk shutdown/restart cycle
```

**Structure Decision**: Single project layout. All changes are within existing `src/` and `tests/` directories. No new directories needed. Two new test files cover the feature's unit and integration requirements.

## Complexity Tracking

> No constitution violations. Table not required.
