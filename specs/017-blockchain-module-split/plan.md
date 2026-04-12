# Implementation Plan: Blockchain Module Split

**Branch**: `017-blockchain-module-split` | **Date**: 2026-04-12 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/017-blockchain-module-split/spec.md`

## Summary

Split the monolithic `Blockchain.cpp` (1,019 lines) into four focused modules: ChainPersistence (persistence/recovery), DifficultyEngine (difficulty calculation/caching), MerkleProofService (proof generation/verification), and a slimmed-down Blockchain core (chain ops, streams, coordination). Modules are owned as member objects via composition; Blockchain core retains all shared state and passes references to modules.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`)
**Storage**: Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`), keys (`keys.dat`), streams (`streams.dat`, `stream_index.dat`)
**Testing**: Catch2, mock objects (MockChunk), each test binary run individually
**Target Platform**: Linux, macOS, Windows (cross-platform)
**Project Type**: Library + CLI executable
**Performance Goals**: Low-latency block lookup; chunk-based architecture (100 blocks/chunk)
**Constraints**: Single-threaded `io_context` execution; no new external dependencies
**Scale/Scope**: ~1,019 lines in Blockchain.cpp → 4 modules each ≤400 lines

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard (C++20) | **PASS** | No language changes; all new files compile under `-std=c++20` |
| II. Build System (Autotools, `-j8`) | **PASS** | New `.cpp` files added to `libblockchain_core_a_SOURCES` in existing `src/Makefile.am` |
| III. Full Test Coverage (Catch2) | **PASS** | Existing tests preserved; new focused tests per extracted module |
| IV. Code Style (match existing) | **PASS** | New files follow existing naming, `#pragma once`, brace placement |
| V. Minimal Dependencies | **PASS** | No new dependencies; uses existing Boost, OpenSSL, nlohmann/json |
| VI. Mandatory TLS | **N/A** | No network changes |
| VII. Cross-Platform | **PASS** | No platform-specific code introduced |
| VIII. Feature Branches with PRs | **PASS** | On branch `017-blockchain-module-split` |
| IX. Pre-1.0 API Stability | **N/A** | Internal refactoring; no API changes |
| X. Low-Latency Performance | **PASS** | No performance regressions; function call delegation only |
| XI. MIT License | **PASS** | New files are project-owned C++ |
| XII. .gitignore Maintenance | **PASS** | No new binaries; existing test binaries unchanged |
| XIII. Roadmap Currency | **PASS** | Will update `docs/ROADMAP.md` on completion |

**Gate result: PASS** — no violations.

## Project Structure

### Documentation (this feature)

```text
specs/017-blockchain-module-split/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (internal module interfaces)
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
src/
├── Blockchain.hpp           # MODIFIED — slimmed core, owns module members
├── Blockchain.cpp           # MODIFIED — core chain ops + stream ops only
├── ChainPersistence.hpp     # NEW — persistence module template header
├── ChainPersistence.cpp     # NEW — persistence module implementation
├── DifficultyEngine.hpp     # NEW — difficulty module header
├── DifficultyEngine.cpp     # NEW — difficulty module implementation
├── MerkleProofService.hpp   # NEW — proof module header
├── MerkleProofService.cpp   # NEW — proof module implementation
├── Block.cpp/hpp            # UNCHANGED
├── MerkleTree.cpp/hpp       # UNCHANGED
├── IBlockchain.hpp          # UNCHANGED
├── Makefile.am              # MODIFIED — add new .cpp files to library sources
└── (all other files)        # UNCHANGED

tests/
├── difficulty_engine_tests.cpp    # NEW — focused difficulty tests
├── chain_persistence_tests.cpp    # NEW — focused persistence tests
├── merkle_proof_tests.cpp         # NEW — focused proof tests
├── (all existing test files)      # UNCHANGED in test logic
└── Makefile.am                    # MODIFIED — add new test binaries
```

**Structure Decision**: Flat file layout in `src/` matching existing convention. No subdirectories introduced. New modules are `.hpp`/`.cpp` pairs added alongside existing files.

## Complexity Tracking

No constitution violations — this section is not applicable.
