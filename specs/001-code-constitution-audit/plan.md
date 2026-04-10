# Implementation Plan: Code Constitution Audit & Remediation

**Branch**: `001-code-constitution-audit` | **Date**: 2026-04-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/001-code-constitution-audit/spec.md`

## Summary

Systematic audit and remediation of the codebase against the project constitution. Fixes 4 constitution violations (C++17→C++20, snakeoil TLS certs, no peer verification, cross-platform paths), 10 bugs (case-sensitive includes, `throw new`, chunk copy-not-ref, silent SSL errors, missing timeouts), consolidates duplicate SSL handshake code, adds thread safety via strand/mutex, optimizes multi-key block retrieval, and introduces structured logging and `.env`-based configuration.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored `src/json.hpp`), Catch2 (test only)
**Storage**: Filesystem — Boost.Serialization binary archives (chunk files `chunk_NNNNNN.dat`, key file `keys.dat`)
**Testing**: Catch2 (`make check`), mock objects (`MockChunk`, `MockSessionHandler`, `MockAcceptor`)
**Target Platform**: Linux, macOS, Windows
**Project Type**: Daemon/library — blockchain node with JSON-RPC and P2P servers
**Performance Goals**: Low-latency query/response; each chunk load ≤ 1 per query; async timeout default 30s
**Constraints**: Minimal dependencies (approved set only); TLS mandatory on all network I/O; no new logging library
**Scale/Scope**: Pre-1.0; ~2k LOC across 20 source files; 2 test files; 2 network server classes

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Status | Notes |
|---|-----------|--------|-------|
| I | Language Standard (C++20) | ✅ PASS | This feature *is* the fix — changes `-std=c++17` → `-std=c++20` |
| II | Build System (Autotools) | ✅ PASS | All changes stay within `configure.ac` and `Makefile.am` |
| III | Full Test Coverage | ✅ PASS | New unit + integration tests required for every user story |
| IV | Code Style | ✅ PASS | Following existing conventions; no style changes introduced |
| V | Minimal Dependencies | ✅ PASS | No new dependencies added; `.env` loader is hand-written |
| VI | Mandatory TLS | ✅ PASS | This feature *strengthens* TLS — adds peer verification and configurable certs |
| VII | Cross-Platform | ✅ PASS | This feature *fixes* Windows path handling |
| VIII | Feature Branches | ✅ PASS | Working on `001-code-constitution-audit` branch |
| IX | Pre-1.0 API Stability | ✅ PASS | Protocol changes are allowed; cert config is additive |
| X | Low-Latency | ✅ PASS | Adds timeouts, optimizes multi-key retrieval |
| XI | MIT License | ✅ PASS | No new third-party code introduced |

**Gate result: PASS** — No violations. Proceeding to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/001-code-constitution-audit/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output (RPC contract)
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code (repository root)

```text
./
├── configure.ac                    # C++20 check (FR-002)
├── src/
│   ├── Makefile.am                 # -std=c++20 (FR-001)
│   ├── main.cpp                    # .env loader, env-var cert config (FR-003)
│   ├── Block.cpp / Block.hpp       # Unchanged
│   ├── Blockchain.cpp / Blockchain.hpp  # Reference fix (FR-010), thread safety (FR-013), optimized query (FR-012)
│   ├── Chunk.cpp / Chunk.hpp       # throw fix (FR-006), path fix (FR-007)
│   ├── IBlockchain.hpp             # Unchanged
│   ├── IChunk.hpp                  # Unchanged
│   ├── MockChunk.hpp               # Unchanged
│   ├── utils.cpp / utils.hpp       # .env loader + structured log helper (FR-003, FR-008)
│   ├── json.hpp                    # Vendored, unchanged
│   └── network/
│       ├── SessionHandler.hpp      # Shared SSL handshake + timeout (FR-011, FR-009)
│       ├── RpcServer.cpp / .hpp    # Delegates handshake to base; server-only TLS (FR-004b)
│       ├── PeerServer.cpp / .hpp   # Delegates handshake to base; mutual TLS (FR-004a)
│       ├── PeerClient.cpp / .hpp   # Unchanged
│       ├── Server.hpp              # Unchanged
│       ├── PacketHeader.hpp        # Unchanged
│       ├── MockAcceptor.hpp        # Unchanged
│       └── MockSessionHandler.hpp  # Updated for new base handshake
├── tests/
│   ├── Makefile.am                 # -std=c++20 (FR-001), case fix (FR-005)
│   ├── block_tests.cpp             # Extended with addBlock data-integrity tests
│   └── server_tests.cpp            # Case fix (FR-005), TLS integration tests
└── .env.example                    # Example env file for cert configuration
```

**Structure Decision**: Single project layout — matches the existing `src/` + `tests/` structure. No new directories needed beyond the spec feature directory.

## Constitution Check — Post-Design Re-evaluation

*Re-checked after Phase 1 design completion.*

| # | Principle | Status | Post-Design Notes |
|---|-----------|--------|-------------------|
| I | Language Standard | ✅ PASS | Uses `std::put_time` + `std::chrono` (C++20 baseline); avoids `std::format` for compiler compat |
| II | Build System | ✅ PASS | Only `configure.ac` and `Makefile.am` modified; `AX_CXX_COMPILE_STDCXX` is standard Autotools |
| III | Full Test Coverage | ✅ PASS | Tests planned for: addBlock integrity, TLS handshake, timeout, paths |
| IV | Code Style | ✅ PASS | Structured log uses `std::cerr`; strand follows existing async patterns |
| V | Minimal Dependencies | ✅ PASS | `.env` loader hand-written; logging via `<chrono>` + `<iomanip>` only |
| VI | Mandatory TLS | ✅ PASS | Strengthened: mutual on P2P, server-only on RPC, configurable certs, error logging |
| VII | Cross-Platform | ✅ PASS | `std::filesystem::path::operator/` for all paths; ifdef for `setenv`/`_putenv_s` |
| VIII | Feature Branches | ✅ PASS | On `001-code-constitution-audit` branch |
| IX | Pre-1.0 API | ✅ PASS | JSON-RPC unchanged; P2P TLS is security improvement |
| X | Low-Latency | ✅ PASS | 30s timeout; chunk-grouped queries; strand = minimal overhead |
| XI | MIT License | ✅ PASS | All code is original; no third-party additions |

**Gate result: PASS** — No violations in the design.
