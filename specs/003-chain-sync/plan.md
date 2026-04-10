# Implementation Plan: Chain Synchronization

**Branch**: `003-chain-sync` | **Date**: 2026-04-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/003-chain-sync/spec.md`

## Summary

Implement chain synchronization so that nodes can download missing blocks from peers. Uses the existing `BLOCKCHAIN_QUERY` and `BLOCKCHAIN_RESPONSE` packet types to exchange chain height information and chunk-aligned block batches over TLS. Sync triggers automatically on peer connection and on demand via a new `requestSync` RPC method. Validation uses existing `isValidNewBlock` per block and `replaceChain` for atomic chain replacement. `addBlock` RPC is blocked during active sync.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored `src/json.hpp`)
**Storage**: Binary chunk files via Boost.Serialization (existing)
**Testing**: Catch2 with mock objects (`MockChunk`, `MockSessionHandler`, `MockAcceptor`)
**Target Platform**: Linux, macOS, Windows (cross-platform per constitution)
**Project Type**: Daemon (P2P node with JSON-RPC interface)
**Performance Goals**: Initial sync of 1,000 blocks < 60s on local network; catch-up of 100 blocks < 10s
**Constraints**: 60-second per-chunk timeout; longest-chain-only fork rule; per-chunk validation rejection
**Scale/Scope**: 2–10 node personal networks; chains of hundreds to low thousands of blocks

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. C++20 | PASS | All new code uses C++20. No later-standard features. |
| II. GNU Autotools | PASS | Changes to `Makefile.am` only. No new build system. |
| III. Full Test Coverage | PASS | Unit tests with Catch2 + mocks for sync logic; network integration tests for P2P sync. |
| IV. Code Style | PASS | Follows existing conventions (naming, `#pragma once`, brace placement). |
| V. Minimal Dependencies | PASS | No new dependencies. Uses Boost.Asio (async), Boost.Serialization (wire format), nlohmann/json (RPC). |
| VI. Mandatory TLS | PASS | All sync communication uses existing TLS-secured P2P channel. |
| VII. Cross-Platform | PASS | Uses only portable Boost.Asio and standard C++20 constructs. |
| VIII. Feature Branches | PASS | Working on `003-chain-sync` branch. |
| IX. Pre-1.0 API | INFO | New `requestSync` RPC method and P2P protocol changes are allowed freely. |
| X. Low-Latency | PASS | Chunk-aligned transfer minimizes round-trips; read-only RPC remains responsive during sync. |
| XI. MIT License | PASS | No new third-party code. |

**Gate result: PASS** — no violations.

## Project Structure

### Documentation (this feature)

```text
specs/003-chain-sync/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── json-rpc.md
│   └── p2p-binary.md
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
src/
├── Block.hpp / .cpp             # Existing — no changes expected
├── Blockchain.hpp / .cpp        # Modified — add getChainBlockCount() accessor
├── IBlockchain.hpp              # Modified — add chain-height query to interface
├── SyncState.hpp                # New — SyncState enum (IDLE, SYNCING) and std::atomic<bool> isSyncing flag
├── ConsensusConfig.hpp          # Existing — no changes expected
├── Chunk.hpp / .cpp             # Existing — no changes expected
├── IChunk.hpp                   # Existing — no changes expected
├── main.cpp                     # Modified — wire sync trigger after PeerClient connects
├── network/
│   ├── PacketHeader.hpp         # Existing — already defines BLOCKCHAIN_QUERY / BLOCKCHAIN_RESPONSE
│   ├── SyncMessages.hpp         # New — SyncQuery and SyncResponse structs with Boost.Serialization
│   ├── PeerClient.hpp / .cpp    # Modified — send BLOCKCHAIN_QUERY on connect, handle responses
│   ├── PeerServer.hpp / .cpp    # Modified — handle BLOCKCHAIN_QUERY, send BLOCKCHAIN_RESPONSE
│   ├── RpcServer.hpp / .cpp     # Modified — add requestSync method, block addBlock during sync
│   ├── Server.hpp               # Existing — no changes expected
│   └── SessionHandler.hpp       # Existing — no changes expected

tests/
├── sync_tests.cpp               # New — unit tests for sync protocol logic
├── server_tests.cpp             # Modified — add requestSync and addBlock-during-sync RPC tests
├── Makefile.am                  # Modified — add sync_tests to test binary
```

**Structure Decision**: Follows the existing flat `src/` and `src/network/` layout. No new directories. Sync logic lives in the existing `PeerClient`/`PeerServer` classes since they already own the P2P connection lifecycle.
