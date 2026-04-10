# Implementation Plan: Peer Discovery & Management

**Branch**: `004-peer-discovery` | **Date**: 2026-04-10 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/004-peer-discovery/spec.md`

## Summary

Implement peer discovery and management so that blockchain nodes can find each other, maintain a healthy set of connections, and survive network disruptions. The feature adds: seed node bootstrap, periodic peer exchange (gossip), configurable connection limits, exponential reconnection backoff, peer list persistence (JSON file with auto-generated node UUID), RPC-based manual peer management, and peer ban/reputation tracking. The design targets up to 100 nodes using the existing Boost.Asio/mTLS stack.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored `src/json.hpp`)
**Storage**: `config.json` (operator configuration, replaces `.env`) and `peers.json` (runtime peer state + node UUID) in blockchain data directory; Boost.Serialization binary archives for P2P wire format
**Testing**: Catch2 with MockChunk, MockAcceptor, MockSessionHandler
**Target Platform**: Linux, macOS, Windows (cross-platform per constitution §VII)
**Project Type**: Daemon / library
**Performance Goals**: Low-latency block queries unaffected; peer exchange is background I/O
**Constraints**: 256-peer storage cap, 8 outbound / 32 inbound connection defaults, 30s exchange interval
**Scale/Scope**: 100-node target network

## Constitution Check

*GATE: PASS — all 11 principles satisfied. No violations.*

| # | Principle | Status |
|---|-----------|--------|
| I | C++20 | ✅ |
| II | GNU Autotools | ✅ |
| III | Full Test Coverage | ✅ |
| IV | Code Style | ✅ |
| V | Minimal Dependencies | ✅ — UUID via `<random>` stdlib, no new deps |
| VI | Mandatory TLS | ✅ — peer exchange over existing mTLS |
| VII | Cross-Platform | ✅ |
| VIII | Feature Branches | ✅ |
| IX | Pre-1.0 API | ✅ — new P2P packet types allowed |
| X | Low-Latency | ✅ — background I/O only |
| XI | MIT License | ✅ |

## Project Structure

### Documentation (this feature)

```text
specs/004-peer-discovery/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
│   ├── json-rpc.md
│   └── p2p-binary.md
└── tasks.md             # Phase 2 output (NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
src/
├── main.cpp                    # Modified: PeerManager integration, config.json loading (replaces .env)
├── NodeConfig.hpp              # NEW: unified node configuration loaded from config.json (replaces loadDotEnv)
├── NodeConfig.cpp              # NEW: JSON config parsing, validation, default generation
├── PeerManager.hpp             # NEW: central peer lifecycle management
├── PeerManager.cpp             # NEW: peer list, connection limits, exchange, bans
├── PeerConfig.hpp              # NEW: peer discovery configuration struct (subset of NodeConfig)
├── network/
│   ├── PacketHeader.hpp        # Modified: new PEER_EXCHANGE, PEER_EXCHANGE_RESPONSE packet types
│   ├── PeerClient.hpp          # Modified: reconnection backoff, UUID exchange
│   ├── PeerClient.cpp          # Modified: reconnection backoff, UUID exchange
│   ├── PeerServer.hpp          # Modified: handle new packet types, inbound limit check
│   ├── PeerServer.cpp          # Modified: handle new packet types, inbound limit check
│   ├── PeerMessages.hpp        # NEW: PeerExchangeRequest, PeerExchangeResponse structs
│   ├── RpcServer.hpp           # Modified: new RPC methods
│   └── RpcServer.cpp           # Modified: addPeer, removePeer, listPeers, banPeer, unbanPeer
├── utils.hpp                   # Modified: remove loadDotEnv declaration
├── utils.cpp                   # Modified: remove loadDotEnv implementation
└── json.hpp                    # Unchanged (vendored nlohmann/json)

tests/
├── node_config_tests.cpp       # NEW: unit tests for config.json loading + default generation
├── peer_manager_tests.cpp      # NEW: unit tests for PeerManager
├── peer_discovery_tests.cpp    # NEW: unit tests for peer exchange protocol
└── server_tests.cpp            # Modified: tests for new RPC methods
```

**Structure Decision**: New `NodeConfig` loads all configuration from `config.json` (replacing `.env` / `loadDotEnv`), providing TLS paths, ports, consensus params, and peer discovery settings in one place. New `PeerManager` owns the runtime peer list (`peers.json`), connection lifecycle, exchange timer, and ban tracking. Both are composed into `main.cpp`; `PeerManager` is referenced by `PeerServer`, `PeerClient`, and `RpcServer`.

## Complexity Tracking

No constitution violations to justify.
