# Roadmap

Last updated: 2026-04-10

## Completed

| Spec | Title | Summary |
|------|-------|---------|
| 001  | Code Constitution Audit & Remediation | C++20 enforcement, TLS hardening, bug fixes, path portability, timeouts, thread safety, dedup network layer, optimized multi-key queries |

## Suggested Specs

### Tier 1 — Core Protocol (required for a functional distributed blockchain)

#### 002 — Consensus Mechanism

No consensus algorithm exists. Blocks are added locally without any distributed agreement. Implement a consensus protocol (e.g., Proof-of-Work, PBFT, or Raft) so that nodes agree on the canonical chain. This is the single biggest gap between the current codebase and a working distributed blockchain.

**Scope**: Define block difficulty/leader election, implement validation rules, add consensus state to `Blockchain`, reject invalid blocks.

---

#### 003 — Chain Synchronization

`BLOCKCHAIN_QUERY` and `BLOCKCHAIN_RESPONSE` packet types are defined in `PacketHeader.hpp` but not implemented. New nodes cannot download the existing chain from peers. Implement chain sync so a joining node can request and reconstruct the full chain from connected peers.

**Scope**: Implement the two reserved packet types, add request/response handlers in `PeerServer`, add sync initiation in `PeerClient`, handle chain-length comparison and chunk-level transfer.

---

#### 004 — Peer Discovery & Management

`PeerClient` can connect to a single hardcoded host:port but there is no mechanism to discover peers, maintain a peer list, reconnect on failure, or limit connections. Implement peer management so nodes can find each other and maintain a healthy network topology.

**Scope**: Peer list persistence, seed node / bootstrap mechanism, peer exchange protocol (gossip), connection limits, reconnection with backoff, peer ban/reputation.

Include an option to disable automatic peer discovery in favor of RPC commands for manual peer management, to support private networks or testing scenarios.

---

#### 005 — Block Propagation & Validation on Receipt

When a `BLOCK` packet arrives via P2P, `PeerServer::do_read_body` deserializes and prints it but does not validate or add it to the chain. Outbound propagation from `PeerClient` is templated but never called from `addBlock`. Implement full block relay so locally-created blocks are broadcast and remotely-received blocks are validated and appended.

**Scope**: Call `IBlockchain::isValidNewBlock()` on received blocks, append valid blocks, relay to other connected peers, deduplicate already-seen blocks, wire `PeerClient::send()` into the `addBlock` path.

---

### Tier 2 — Data Integrity & Persistence (required for production reliability)

#### 006 — Transaction Model

`Block::data` is an opaque string. There is no structured transaction format, no sender/recipient, no digital signature verification. Define a transaction schema so blocks carry verifiable, structured payloads.

**Scope**: Transaction struct with sender, recipient, payload, signature fields; Ed25519 or ECDSA signature creation/verification; transaction validation before block inclusion; update `addBlock` RPC params.

---

#### 007 — Multi-Chunk Persistence & Recovery

Only chunk 0 is saved on shutdown (`bc.saveChunk(0)`). A chain with >100 blocks would lose all chunks beyond the first. Implement automatic chunk persistence as they fill, and recovery of the full chain on startup.

**Scope**: Auto-save chunks when they reach capacity, load all existing chunk files on startup, detect and report corrupted chunk files, add `getChainLength` / `getChunkCount` to `IBlockchain`.

---

#### 008 — Merkle Tree & Block Header Optimization

No Merkle root or lightweight block header exists. Verification requires loading full block data. Add Merkle tree computation per chunk or block batch to enable efficient proof-of-inclusion and light-client support.

**Scope**: Merkle root field in block header, tree construction on chunk finalization, proof generation and verification API.

---

### Tier 3 — Operations & Usability

#### 009 — CLI & Configuration

The daemon takes a single positional argument (blockchain directory). Ports are hardcoded (12345/12346). There is no way to configure the peer list, log level, or other runtime parameters without editing code. Add a proper CLI and config system.

**Scope**: Command-line argument parsing (port, peer list, log level), extend `.env` support, validate all config at startup, `--help` output.

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

`README.md` contains a single heading. The `.env.example`, quickstart, and contract docs exist in the spec directory but are not surfaced to developers. Write user-facing documentation.

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
004 Peer Discovery ─────┼── Tier 1: unlocks distributed operation
005 Block Propagation ──┘
006 Transaction Model ──┐
007 Multi-Chunk Persist ┼── Tier 2: data integrity for real use
008 Merkle Tree ────────┘
009 CLI & Config ───────┐
010 RPC Expansion ──────┼── Tier 3: usability
011 Graceful Lifecycle ─┘
012 Integration Tests ──┐
013 CI/CD Pipeline ─────┼── Tier 4: quality
014 Documentation ──────┘
015–017 ────────────────── Tier 5: future
```

Specs within each tier can be worked in parallel where noted. Tier 1 specs have internal ordering: 002 (consensus) should land first since 003 and 005 depend on knowing the consensus rules. 004 (peer discovery) is independent and can proceed in parallel with 002.
