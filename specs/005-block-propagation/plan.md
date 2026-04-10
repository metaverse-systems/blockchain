# Implementation Plan: Block Propagation & Validation on Receipt

**Branch**: `005-block-propagation` | **Date**: 2026-04-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/005-block-propagation/spec.md`

## Summary

Implement block propagation so that locally-created blocks are broadcast to all connected peers, remotely-received blocks are validated against consensus rules and appended to the chain, and valid blocks are relayed onward to other peers. Adds deduplication via a recent-block hash cache, a bounded pending pool for gap blocks, per-peer rate limiting on inbound BLOCK packets, and sync-aware queueing of propagated blocks.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`)
**Storage**: Boost.Serialization binary chunk files (existing)
**Testing**: Catch2 (unit + integration via `make check`)
**Target Platform**: Linux, macOS, Windows (cross-platform per constitution)
**Project Type**: Daemon / library
**Performance Goals**: ≥10 blocks/sec sustained propagation throughput; ≤5s single-hop latency
**Constraints**: Bounded memory for caches/pools; no new external dependencies
**Scale/Scope**: Peer mesh of 10–50 nodes, chunks of 100 blocks each

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Language Standard (C++20) | PASS | All new code will use C++20; no later features |
| II. Build System (Autotools) | PASS | Changes to existing Makefile.am only |
| III. Full Test Coverage | PASS | Unit tests (Catch2 + mocks) and integration tests planned |
| IV. Code Style | PASS | Follow existing conventions |
| V. Minimal Dependencies | PASS | No new dependencies; uses existing Boost, OpenSSL, nlohmann/json |
| VI. Mandatory TLS | PASS | All block propagation uses existing TLS P2P channels |
| VII. Cross-Platform | PASS | Standard C++20 + Boost only; no platform-specific APIs |
| VIII. Feature Branches | PASS | Working on `005-block-propagation` branch |
| IX. Pre-1.0 API Stability | PASS | P2P protocol changes allowed pre-1.0 |
| X. Low-Latency Performance | PASS | Hash-keyed cache O(1) lookup; bounded pools |
| XI. MIT License | PASS | No third-party code introduced |

**Gate result**: ALL PASS — proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/005-block-propagation/
├── plan.md
├── research.md
├── data-model.md
├── quickstart.md
├── contracts/
│   ├── json-rpc.md
│   └── p2p-binary.md
└── tasks.md
```

### Source Code (repository root)

```text
src/
├── Block.hpp / Block.cpp                   # Existing — no changes
├── Blockchain.hpp / Blockchain.cpp         # Modified — new appendBlock() method for pre-mined blocks
├── IBlockchain.hpp                         # Modified — new virtual appendBlock() method
├── BlockPropagation.hpp                    # NEW — propagation engine (validate, relay, dedup, queue)
├── BlockPropagation.cpp                    # NEW
├── PeerManager.hpp / PeerManager.cpp       # Modified — broadcast_block(), relay helper
├── ConsensusConfig.hpp                     # Existing — no changes
├── SyncState.hpp                           # Existing — no changes
├── network/
│   ├── PeerServer.cpp                      # Modified — BLOCK case calls propagation engine
│   ├── PeerClient.hpp / PeerClient.cpp     # Modified — BLOCK case calls propagation engine
│   ├── RpcServer.cpp                       # Modified — addBlock triggers broadcast
│   └── PacketHeader.hpp                    # Existing — BLOCK type already defined
tests/
├── block_propagation_tests.cpp             # NEW — unit tests for BlockPropagation
├── Makefile.am                             # Modified — add new test binary
```

**Structure Decision**: Single-project flat layout matching existing codebase. New `BlockPropagation` class in `src/` alongside existing components. No new directories.
