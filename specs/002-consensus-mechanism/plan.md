# Implementation Plan: Consensus Mechanism

**Branch**: `002-consensus-mechanism` | **Date**: 2026-04-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/002-consensus-mechanism/spec.md`

## Summary

Add a Proof-of-Work consensus mechanism to the blockchain. Blocks gain `nonce` and `difficulty` fields; mining computes a nonce such that the block hash has the required number of leading zero bits. Validation rejects blocks with invalid proofs. The longest-valid-chain rule resolves forks (with a configurable max reorg depth). Difficulty adjusts periodically based on block production rate. The implementation extends the existing `Block` and `IBlockchain` types without breaking the RPC API. Serialization format is extended with new fields (no legacy data exists).

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`)
**Storage**: Binary chunk files via Boost.Serialization (existing)
**Testing**: Catch2 (`make check`)
**Target Platform**: Linux, macOS, Windows (cross-platform required by constitution)
**Project Type**: Daemon / library
**Performance Goals**: Sub-second mining at personal-network difficulty levels (SC-007); low-latency block lookup preserved (constitution §X)
**Constraints**: Single-threaded mining; 30s mining timeout default; no new dependencies
**Scale/Scope**: 2–10 trusted peers per personal network; difficulty will remain low in practice

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| §I C++20 | ✅ PASS | All new code uses C++20. No later features. |
| §II GNU Autotools | ✅ PASS | New `.cpp`/`.hpp` files added to existing `Makefile.am`. No build system change. |
| §III Full Test Coverage | ✅ PASS | Unit tests for mining, validation, difficulty; integration tests for chain replacement. Catch2 + mocks. |
| §IV Code Style | ✅ PASS | Follow existing conventions (naming, indentation, `#pragma once`). |
| §V Minimal Dependencies | ✅ PASS | No new dependencies. SHA-256 already available via OpenSSL EVP. |
| §VI Mandatory TLS | ✅ PASS | No network layer changes in this spec. TLS unaffected. |
| §VII Cross-Platform | ✅ PASS | No platform-specific code. `uint64_t` nonce, `std::chrono` timestamps, standard C++20. |
| §VIII Feature Branches | ✅ PASS | On branch `002-consensus-mechanism`. |
| §IX Pre-1.0 API Stability | ✅ PASS | Block struct extension is allowed. RPC response gains new fields (backward compatible). |
| §X Low-Latency | ✅ PASS | Mining is bounded by timeout. Query paths unchanged. Chunk architecture preserved. |
| §XI MIT License | ✅ PASS | No third-party code added. |

**Gate result: ALL PASS — proceed to Phase 0.**

## Project Structure

### Documentation (this feature)

```text
specs/002-consensus-mechanism/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   └── json-rpc.md      # Updated RPC contract for consensus fields
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
src/
├── Block.hpp            # Extended: add nonce, difficulty fields
├── Block.cpp            # Extended: calculateHash includes nonce/difficulty
├── Blockchain.hpp       # Extended: consensus params, chain replacement
├── Blockchain.cpp       # Extended: mining loop, difficulty adjustment, validation
├── IBlockchain.hpp      # Extended: isValidNewBlock with PoW checks
├── ConsensusConfig.hpp  # NEW: configurable consensus parameters
├── utils.hpp            # Unchanged (sha256 already available)
├── utils.cpp            # Unchanged
├── network/
│   └── RpcServer.cpp    # Updated: addBlock response includes new fields
tests/
├── block_tests.cpp      # Extended: PoW validation tests
├── consensus_tests.cpp  # NEW: mining, difficulty adjustment, chain replacement tests
├── Makefile.am          # Updated: add consensus_tests
```

**Structure Decision**: Single-project flat layout following existing conventions. New consensus logic lives in the existing `Block`/`Blockchain` types. One new header (`ConsensusConfig.hpp`) for tunable parameters. One new test file (`consensus_tests.cpp`) for consensus-specific tests.

## Complexity Tracking

> No constitution violations. Table not needed.
