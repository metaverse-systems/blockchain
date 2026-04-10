# Implementation Plan: Multi-Chunk Persistence & Recovery

**Branch**: `007-multi-chunk-persistence` | **Date**: 2026-04-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/007-multi-chunk-persistence/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

Implement automatic persistence of all blockchain chunks (not just chunk 0), periodic saving of the active chunk on a configurable timer, full chain recovery on startup with corruption detection, on-demand loading of filled chunks from disk, and chain-replace archival to a timestamped backup directory. Adds `getChainLength`/`getChunkCount` to `IBlockchain` and a `persistence.save_interval_seconds` field to `config.json`.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`)
**Storage**: Boost.Serialization binary archives for chunk files (`chunk_NNNNNN.dat`), keys (`keys.dat`), streams (`streams.dat`, `stream_index.dat`) in the blockchain data directory
**Testing**: Catch2 (`make check`), mock objects (`MockChunk`, `MockBlockchain`, `MockSessionHandler`, `MockAcceptor`)
**Target Platform**: Linux, macOS, Windows (cross-platform per constitution §VII)
**Project Type**: Library + daemon
**Performance Goals**: Chunk auto-save < 2s, startup recovery < 30s for 10k blocks, low-latency block queries (constitution §X)
**Constraints**: On-demand chunk loading — only active chunk resident in memory; filled chunks loaded/freed per query
**Scale/Scope**: Chains with thousands of chunks; chunk size fixed at 100 blocks

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard (C++20) | PASS | All new code uses C++20 features only |
| II. Build System (Autotools) | PASS | Changes to `Makefile.am` only; no new build system |
| III. Full Test Coverage | PASS | Unit tests for chunk lifecycle, corruption detection, periodic save; integration tests for multi-chunk recovery |
| IV. Code Style | PASS | Follows existing conventions (`#pragma once`, naming, indentation) |
| V. Minimal Dependencies | PASS | No new dependencies — uses Boost.Asio timers (already approved), `<filesystem>`, `<chrono>` from C++20 stdlib |
| VI. Mandatory TLS | N/A | No network protocol changes |
| VII. Cross-Platform | PASS | `std::filesystem`, `std::chrono`, Boost.Asio timers are cross-platform; no platform-specific code |
| VIII. Feature Branches | PASS | Working on `007-multi-chunk-persistence` branch |
| IX. Pre-1.0 API Stability | PASS | Adding new interface methods (`getChainLength`, `getChunkCount`) is allowed |
| X. Low-Latency Performance | PASS | On-demand chunk loading keeps memory bounded; chunk file I/O is off hot path for in-memory active chunk |
| XI. MIT License | PASS | No new third-party code |

**GATE RESULT: PASS** — No violations.

## Project Structure

### Documentation (this feature)

```text
specs/007-multi-chunk-persistence/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (N/A — no new external APIs)
└── tasks.md             # Phase 2 output
```

### Source Code (repository root)

```text
src/
├── Blockchain.hpp       # MODIFY — add dirty tracking, periodic save timer, multi-chunk save/load, archive on replaceChain
├── Blockchain.cpp       # MODIFY — implement all new chunk lifecycle methods
├── Chunk.hpp            # MODIFY — minor (no changes expected)
├── Chunk.cpp            # MODIFY — minor (no changes expected)
├── IBlockchain.hpp      # MODIFY — add getChainLength, getChunkCount virtual methods
├── IChunk.hpp           # NO CHANGE
├── NodeConfig.hpp       # MODIFY — add PersistenceConfig struct
├── NodeConfig.cpp       # MODIFY — parse/validate persistence section from config.json
├── main.cpp             # MODIFY — update startup sequence (multi-chunk discovery/load), shutdown (save all chunks), start periodic timer

tests/
├── chunk_persistence_tests.cpp     # NEW — unit tests for auto-save on fill, periodic save, dirty tracking
├── chunk_recovery_tests.cpp        # NEW — unit tests for multi-chunk startup, corruption detection, contiguous prefix
├── chunk_replace_tests.cpp         # NEW — unit tests for replaceChain archive behavior
├── MockBlockchain.hpp              # MODIFY — add getChainLength, getChunkCount stubs
```

**Structure Decision**: All changes are within the existing `src/` and `tests/` directories. No new directories under `src/`. The `backups/` directory is created at runtime within the blockchain data directory.

## Complexity Tracking

No constitution violations — section not applicable.
