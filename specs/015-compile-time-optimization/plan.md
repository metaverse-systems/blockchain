# Implementation Plan: Compile-Time Optimization

**Branch**: `015-compile-time-optimization` | **Date**: 2026-04-12 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/015-compile-time-optimization/spec.md`

## Summary

Eliminate redundant compilation by extracting the 11 core source files into a shared static archive (`libblockchain_core.a`) built once in `src/Makefile.am`. The main binary and all 13 test binaries link against the archive instead of each independently compiling the same sources. This reduces compilation units from ~177 to ~34 (~81% reduction).

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)  
**Primary Dependencies**: Boost (Asio, Serialization, Program Options), OpenSSL, nlohmann/json (vendored), Catch2 (test)  
**Storage**: N/A (build-system-only change)  
**Testing**: Catch2 via `make -j8 check`; 13 test binaries run individually  
**Target Platform**: Linux (GCC, Clang), macOS (Clang), Windows (MinGW-w64/MSYS2)  
**Project Type**: CLI / P2P daemon  
**Performance Goals**: ≥50% reduction in clean compile time from 13:20 baseline  
**Constraints**: Must use GNU Autotools (constitution Principle II); no libtool  
**Scale/Scope**: 11 core .cpp files, 1 main binary, 13 test binaries, 2 Makefile.am files modified

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard | PASS | No code changes; C++20 flags preserved |
| II. Build System | PASS | Remains GNU Autotools; `make -j8` preserved |
| III. Full Test Coverage | PASS | No test files added/removed; all existing tests preserved |
| IV. Code Style | PASS | No source code changes |
| V. Minimal Dependencies | PASS | No new dependencies; libtool intentionally avoided |
| VI. Mandatory TLS | PASS | No network changes |
| VII. Cross-Platform Support | PASS | `noinst_LIBRARIES` works on all three CI platforms |
| VIII. Feature Branches | PASS | On branch `015-compile-time-optimization` |
| IX. Pre-1.0 API Stability | PASS | No API changes |
| X. Low-Latency Performance | PASS | No runtime code changes |
| XI. MIT License | PASS | No new files with license implications |
| XII. .gitignore Maintenance | PASS | `libblockchain_core.a` is a build artifact in `src/`; must be added to `.gitignore` |
| XIII. Roadmap Currency | PASS | ROADMAP.md update required on completion |

**Post-Phase-1 Re-check**: All gates still PASS. The design introduces `libblockchain_core.a` as a new build artifact requiring a `.gitignore` entry (XII).

## Project Structure

### Documentation (this feature)

```text
specs/015-compile-time-optimization/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks command)
```

### Source Code (files modified)

```text
src/
├── Makefile.am          # MODIFIED: add noinst_LIBRARIES, refactor blockchain target
tests/
├── Makefile.am          # MODIFIED: all 13 targets drop ../src/*.cpp, link archive
.gitignore               # MODIFIED: add src/libblockchain_core.a
docs/
└── ROADMAP.md           # MODIFIED: on completion
```

**Structure Decision**: Existing `src/` + `tests/` layout preserved. No new directories. The static archive is built in `src/` alongside the existing binary.

## Complexity Tracking

No constitution violations. Table not applicable.
