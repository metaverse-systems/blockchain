# Roadmap

Last updated: 2026-04-11

## Completed

| Spec | Title | Summary |
|------|-------|---------|
| 001  | Code Constitution Audit & Remediation | C++20 enforcement, TLS hardening, bug fixes, path portability, timeouts, thread safety, dedup network layer, optimized multi-key queries |
| 002  | Consensus Mechanism | Proof-of-work validation, mining with configurable timeout, longest-valid-chain resolution with max reorg depth, automatic difficulty adjustment |
| 003  | Chain Synchronization | Full-chain and incremental sync via `BLOCKCHAIN_QUERY`/`BLOCKCHAIN_RESPONSE`, per-chunk validation with error recovery, graceful timeout/reconnection handling, RPC integration (`requestSync`, addBlock blocking during sync) — 3 manual quickstart validation tasks remaining |
| 004  | Peer Discovery & Management | Seed-node bootstrap, peer exchange gossip, exponential-backoff reconnection, inbound/outbound connection limits, RPC manual peer management, reputation tracking with auto-ban, `config.json`/`peers.json` persistence with node UUID |
| 005  | Block Propagation & Validation on Receipt | Outbound broadcast of locally-mined blocks, consensus validation on receipt, multi-hop relay, duplicate suppression via recent-block cache, per-peer rate limiting, invalid-block sender penalties, gap-block pending pool, sync-aware queueing |
| 006  | Transaction Model (Stream-Based Key/Value Store) | Structured `StreamEntry` arrays replace opaque block data, named-stream publish via RPC, history and latest-mode queries, explicit/implicit stream creation, per-node publish permissions via `config.json`, P2P validation, 128 MB entry size cap, base64 binary support |
| 007  | Multi-Chunk Persistence & Recovery | Auto-save chunks when they reach capacity, load all existing chunk files on startup, detect and report corrupted chunk files, `getChainLength`/`getChunkCount` on `IBlockchain`, startup chain recovery |
| 008  | Merkle Tree & Block Header Optimization | Per-block Merkle root (RFC 6962 domain-separated SHA-256), `getInclusionProof`/`verifyInclusionProof`/`getBlockHeader` JSON-RPC endpoints, O(log n) inclusion proofs, lightweight header-only block view, block hash now incorporates Merkle root instead of serialized entries |

## Suggested Specs

### Tier 2 — Data Integrity & Persistence (required for production reliability)

*All Tier 2 specs completed.*

---

### Tier 3 — Operations & Usability

#### 009 — CLI & Configuration

The daemon takes a single positional argument (blockchain directory). Ports are hardcoded (12345/12346). There is no way to configure the peer list, log level, or other runtime parameters without editing code. Add a proper CLI and config system.

**Scope**: Command-line argument parsing (port, peer list, log level), extend `config.json` support, validate all config at startup, `--help` output.

---

#### 010 — RPC API Expansion

The JSON-RPC server exposes only `addBlock`, `getBlockByIndex`, and `getBlocksByKeys`. Operators and clients need introspection and management endpoints. Add status, chain info, and peer management RPCs.

**Scope**: `getChainLength`, `getChunkCount`, `getNodeStatus`, `getPeers`, `addPeer`, `getBlockRange` methods; JSON-RPC error codes per spec.

---

#### 011 — Graceful Multi-Chunk Shutdown & Startup

Signal handler saves only chunk 0 and keys. Multi-chunk chains lose unsaved data on shutdown. Startup loads only chunk 0. Fix the lifecycle to persist and restore the full chain state.

**Scope**: Iterate all dirty chunks on shutdown, track dirty state per chunk, sequential chunk load on startup, startup integrity check.

---

### Tier 4 — Quality & DevOps

#### 012 — Integration Test Suite

Tests are limited to unit-level Catch2 tests for blocks and server construction. No end-to-end tests exercise the RPC or P2P protocols over real TLS connections. Add integration tests for multi-node scenarios.

**Scope**: Catch2 integration test binary, TLS test fixtures with self-signed certs, RPC client test helper, P2P two-node sync test, `make check` integration.

---

#### 013 — CI/CD Pipeline

No CI configuration exists. The constitution requires cross-platform support (Linux, macOS, Windows) but there is no automated verification. Add CI for all three platforms.

**Scope**: GitHub Actions workflow, matrix build (Linux gcc/clang, macOS clang, Windows MSVC), `make check` on all platforms, artifact caching for Boost/OpenSSL.

---

#### 014 — Documentation & Developer Guide

`README.md` contains a single heading. The quickstart and contract docs exist in the spec directory but are not surfaced to developers. Write user-facing documentation.

**Scope**: README with build instructions, architecture overview, configuration guide, RPC API reference, contributing guide.

---

### Tier 5 — Future / Exploratory

#### 015 — Smart Contract / Scripting Layer

No programmability exists beyond storing opaque data strings. Explore adding a scripting or smart contract layer for programmable on-chain logic.

---

#### 016 — Monitoring, Metrics & Health Endpoint

No observability. Add a `/health` HTTP endpoint, Prometheus-compatible metrics (block height, peer count, chunk loads), and structured log leveling.

---

#### 017 — Light Client Protocol

No way to verify block inclusion without downloading the full chain. Design a light client protocol using Merkle proofs (depends on 008) for resource-constrained participants.

---

## Suggested Priority Order

```
002 Consensus ──────────┐
003 Chain Sync ─────────┤
004 Peer Discovery ─────┼── Tier 1: COMPLETE ✓
005 Block Propagation ──┘
006 Transaction Model ──┐
007 Multi-Chunk Persist ┼── Tier 2: 006 complete, 007–008 next
008 Merkle Tree ────────┘
009 CLI & Config ───────┐
010 RPC Expansion ──────┼── Tier 3: usability
011 Graceful Lifecycle ─┘
012 Integration Tests ──┐
013 CI/CD Pipeline ─────┼── Tier 4: quality
014 Documentation ──────┘
015–017 ────────────────── Tier 5: future
```

Tier 1 is complete. 006 (Transaction Model) is complete, making 007 (Multi-Chunk Persistence) the next priority — it is the most impactful remaining spec since chains with >100 blocks currently lose data. 008 (Merkle Tree) can follow or run in parallel. Tier 3 specs (009–011) are independent and can proceed alongside Tier 2.
