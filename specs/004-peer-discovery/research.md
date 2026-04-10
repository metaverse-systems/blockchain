# Research: Peer Discovery & Management

**Feature**: 004-peer-discovery
**Date**: 2026-04-10

## R1: Peer Exchange Wire Format

**Decision**: Add two new packet types `PEER_EXCHANGE` and `PEER_EXCHANGE_RESPONSE` to `PacketHeader.hpp`. Payloads use Boost.Serialization binary archives, matching the existing P2P wire format established in specs 002 and 003.

**Rationale**: The existing P2P protocol uses `PacketHeader` (length + type) followed by Boost.Serialization binary payloads for `BLOCK`, `BLOCKCHAIN_QUERY`, and `BLOCKCHAIN_RESPONSE`. Using the same serialization for peer exchange messages keeps the codebase consistent and avoids introducing a second wire format. Constitution §V (minimal dependencies) prohibits adding Protobuf or similar.

**Alternatives considered**:
- JSON over TLS: Rejected — inconsistent with the existing binary P2P protocol. The RPC channel uses JSON but the P2P channel should remain binary for consistency and compactness.
- Embedding peer lists in existing BLOCKCHAIN_QUERY/RESPONSE: Rejected — conflates two concerns (chain sync vs. peer discovery). Separate packet types are clearer and allow independent timing.

**Wire format**:
```
PEER_EXCHANGE payload:
  string     sender_uuid          // The sending node's UUID
  uint16_t   sender_listen_port   // The P2P listen port of the sender
  uint16_t   peer_count           // Number of peer entries
  vector<PeerEntry>:
    string   host                 // IPv4 or IPv6 address
    uint16_t port                 // P2P listen port
    string   node_uuid            // UUID of that peer (if known, empty otherwise)

PEER_EXCHANGE_RESPONSE payload:
  string     sender_uuid          // The responding node's UUID
  uint16_t   sender_listen_port   // The P2P listen port of the responder
  uint16_t   peer_count
  vector<PeerEntry>:
    string   host
    uint16_t port
    string   node_uuid
```

## R2: Node UUID Generation Without External Dependencies

**Decision**: Generate UUIDs using C++20 `<random>` with `std::random_device` seeding a Mersenne Twister, formatted as a standard UUID v4 string (8-4-4-4-12 hex with version/variant bits set).

**Rationale**: Constitution §V prohibits unapproved dependencies. The C++ standard library provides sufficient randomness for generating unique node identifiers. UUID v4 is a well-understood format that is easy to compare, log, and exchange. A cryptographic UUID is not required — the UUID is an identifier, not a secret. For collision resistance, 122 random bits are more than sufficient for a 100-node network.

**Alternatives considered**:
- Boost.UUID: Rejected — Boost.UUID is not in the approved dependency list (only Asio and Serialization are listed). Adding it would require a constitution amendment.
- TLS certificate fingerprint as identity: Rejected — couples node identity to certificate lifecycle. Certificate rotation would change the node's identity, breaking duplicate connection detection and peer tracking.
- Raw random bytes (not UUID format): Rejected — UUID v4 format is human-readable in logs and JSON, and has well-defined comparison semantics.

**Implementation sketch**:
```cpp
std::string generate_uuid_v4() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    uint64_t hi = dist(gen);
    uint64_t lo = dist(gen);
    // Set version (4) and variant (10xx) bits
    hi = (hi & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    lo = (lo & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;
    // Format as 8-4-4-4-12
    // ...
}
```

## R3: config.json Schema and .env Migration

**Decision**: Introduce `config.json` as the single source of all node configuration, replacing `.env`. If `config.json` does not exist on startup, generate a default one. The `.env` format and `loadDotEnv` are removed entirely — no migration code.

**Rationale**: The `.env` format is flat key-value and does not support structured data (e.g., seed node lists, nested peer config). JSON is already a project dependency (nlohmann/json vendored) and supports arrays and objects natively. A clean break avoids migration complexity; existing deployments recreate config via the generated default file.

**Alternatives considered**:
- Keep `.env` for existing settings, add `config.json` only for new settings: Rejected — splits configuration across two files and two parsers.
- TOML or YAML config: Rejected — would require a new dependency (constitution §V).
- Command-line arguments only: Rejected — doesn't support complex structures like seed node lists well, and the spec defers CLI to spec 009.
- Automatic .env migration: Rejected by operator preference — not worth the code for a pre-1.0 project.

**Schema** (default values shown):
```json
{
  "tls": {
    "cert_file": "cert.pem",
    "key_file": "key.pem",
    "ca_file": ""
  },
  "network": {
    "rpc_port": 12345,
    "p2p_port": 12346,
    "timeout_seconds": 30
  },
  "consensus": {
    "target_block_interval": 10,
    "adjustment_window": 10,
    "max_adjustment_factor": 4.0,
    "min_difficulty": 1,
    "max_difficulty": 16,
    "initial_difficulty": 1,
    "mining_timeout": 30,
    "max_future_timestamp": 120,
    "max_reorg_depth": 100
  },
  "peers": {
    "seed_nodes": [],
    "max_outbound": 8,
    "max_inbound": 32,
    "exchange_interval_seconds": 30,
    "discovery_enabled": true,
    "max_stored_peers": 256,
    "reconnect_base_delay_seconds": 5,
    "reconnect_max_delay_seconds": 300,
    "ban_threshold_errors": 10,
    "ban_duration_seconds": 3600
  }
}
```

No `.env` migration — clean break. Operators create `config.json` manually or use the auto-generated default.

## R4: Peer List Persistence Format (peers.json)

**Decision**: Store runtime peer state in `peers.json` in the blockchain data directory. This file is managed by the daemon, not by the operator. Write atomically (write to temp file, then rename) to survive unclean shutdowns.

**Rationale**: Separating operator config (`config.json`) from runtime state (`peers.json`) prevents the daemon from overwriting operator settings. Atomic writes via rename are POSIX-portable and supported on all target platforms via `std::filesystem::rename`.

**Alternatives considered**:
- SQLite: Rejected — new dependency (constitution §V).
- Append-only log: Rejected — requires compaction logic. A single JSON file is simpler for a 256-entry cap.
- Binary format (Boost.Serialization): Rejected — harder to inspect and debug. JSON is human-readable and already vendored.

**Schema**:
```json
{
  "node_uuid": "550e8400-e29b-41d4-a716-446655440000",
  "peers": [
    {
      "host": "192.168.1.10",
      "port": 12346,
      "node_uuid": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
      "last_seen": 1712700000,
      "error_count": 0
    }
  ],
  "bans": [
    {
      "host": "10.0.0.5",
      "port": 12346,
      "reason": "excessive_errors",
      "expires": 1712703600
    }
  ]
}
```

## R5: Connection Management and Duplicate Detection

**Decision**: `PeerManager` maintains two connection sets: `outbound_connections` (initiated by this node) and `inbound_connections` (accepted by `PeerServer`). Duplicate detection occurs after TLS handshake: both sides exchange their node UUID as the first message on a new `PEER_EXCHANGE`. If a connection to the same UUID already exists, the node with the lower UUID keeps its outbound connection and the other drops its outbound.

**Rationale**: UUID comparison is deterministic and symmetric — both nodes will independently reach the same conclusion about which connection to keep, without requiring an explicit negotiation round. Exchanging UUIDs immediately after handshake (before any other protocol messages) ensures dedup happens before any state is affected.

**Alternatives considered**:
- IP-based dedup only: Rejected — NAT and load balancers can make the same node appear at different addresses. UUID-based detection is reliable.
- Dedicated handshake packet: Considered but the `PEER_EXCHANGE` message already carries the sender's UUID. Sending a peer exchange immediately after handshake serves dual purposes: identify the node AND share the peer list.

**Flow**:
```
Node A connects to Node B (outbound from A, inbound to B):
  1. TLS handshake completes
  2. A sends PEER_EXCHANGE (includes A's UUID + A's peer list)
  3. B receives PEER_EXCHANGE, records A's UUID
  4. B checks: do I already have a connection to UUID A?
     - If yes: compare UUIDs. Lower UUID keeps outbound. B drops duplicate.
     - If no: add A to connected peers.
  5. B sends PEER_EXCHANGE_RESPONSE (includes B's UUID + B's peer list)
  6. A receives response, records B's UUID, performs same dedup check
```

## R6: Exponential Backoff Strategy

**Decision**: Use a simple doubling backoff: base delay (default 5s) → 10s → 20s → 40s → ... up to max delay (default 300s). Add ±20% jitter to avoid thundering herd. Reset to base delay on successful connection.

**Rationale**: Exponential backoff is the standard approach for transient network failures. Jitter prevents multiple nodes from synchronizing their retry attempts. The defaults (5s base, 300s max) are reasonable for a 100-node network — a node will retry at most once every 5 minutes even in the worst case.

**Alternatives considered**:
- Linear backoff: Rejected — recovers too slowly from sustained outages without being aggressive enough for transient failures.
- Fibonacci backoff: Rejected — unnecessary complexity for marginal benefit.
- No backoff (fixed interval): Rejected — can overwhelm a recovering peer with rapid reconnection attempts.

## R7: Peer Exchange Timing and Bootstrap

**Decision**: On new connection, immediately exchange peer lists (serves as both UUID handshake and initial peer discovery). Then exchange periodically every 30 seconds (configurable). Seed nodes are treated as normal peers after initial connection — no special ongoing behavior.

**Rationale**: Immediate exchange on connect means a new node gets a full peer list within seconds of joining, satisfying SC-001 (60-second discovery). Periodic exchange propagates topology changes (new nodes, departures) across the network. Treating seeds as regular peers after bootstrap simplifies the code — no separate "seed node protocol."

**Alternatives considered**:
- Exchange only on demand (RPC-triggered): Rejected — would not satisfy SC-003 (automatic full awareness in 3 minutes).
- Adaptive interval (faster when network is changing): Rejected — over-engineering for a 100-node target. Fixed interval is simple and predictable.
