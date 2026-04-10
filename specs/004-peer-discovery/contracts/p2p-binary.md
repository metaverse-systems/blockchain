# P2P Binary Protocol Contract: Peer Discovery & Management

**Feature**: 004-peer-discovery
**Date**: 2026-04-10
**Base contract**: [003 P2P binary contract](../../003-chain-sync/contracts/p2p-binary.md)

This document describes **only the changes** to the P2P binary protocol introduced by peer discovery and management. All existing packet types and behaviors are unchanged.

## New Packet Types

### PacketType Enum Update

| Value | Name | Status |
|-------|------|--------|
| 0 | `BLOCK` | Existing |
| 1 | `BLOCKCHAIN_QUERY` | Existing |
| 2 | `BLOCKCHAIN_RESPONSE` | Existing |
| 3 | `PEER_EXCHANGE` | **New** |
| 4 | `PEER_EXCHANGE_RESPONSE` | **New** |

### PEER_EXCHANGE

**Direction**: Bidirectional (either side may initiate)

**Packet header**:
| Field | Value |
|-------|-------|
| `type` | `PacketType::PEER_EXCHANGE` (3) |
| `length` | Size of serialized payload |

**Payload** (Boost.Serialization binary archive):
| Field | Type | Description |
|-------|------|-------------|
| `sender_uuid` | `string` | UUID of the sending node |
| `sender_listen_port` | `uint16_t` | P2P listen port of the sender |
| `peers` | `vector<PeerAddress>` | Full list of non-banned known peer addresses |

Each `PeerAddress` contains:
| Field | Type | Description |
|-------|------|-------------|
| `host` | `string` | IPv4 or IPv6 address |
| `port` | `uint16_t` | P2P listen port |

**Trigger**:
1. Sent immediately after TLS handshake completes on a new connection (both inbound and outbound).
2. Sent periodically by the exchange timer (default every 30 seconds) on all active connections.

### PEER_EXCHANGE_RESPONSE

**Direction**: Reply to a `PEER_EXCHANGE`

**Packet header**:
| Field | Value |
|-------|-------|
| `type` | `PacketType::PEER_EXCHANGE_RESPONSE` (4) |
| `length` | Size of serialized payload |

**Payload** (Boost.Serialization binary archive):
| Field | Type | Description |
|-------|------|-------------|
| `sender_uuid` | `string` | UUID of the responding node |
| `sender_listen_port` | `uint16_t` | P2P listen port of the responder |
| `peers` | `vector<PeerAddress>` | Full list of non-banned known peer addresses |

**Trigger**: Sent in reply to every received `PEER_EXCHANGE`.

## Protocol Flows

### New Connection (Outbound)

```
Node A (outbound)                        Node B (inbound / PeerServer)
  │                                          │
  │──── TLS handshake ─────────────────────►│
  │◄─── TLS handshake complete ────────────│
  │                                          │
  │──── PEER_EXCHANGE ─────────────────────►│
  │     { uuid: A, port: 12346,             │
  │       peers: [{10.0.0.2:12346}, ...] }  │
  │                                          │
  │     B records A's UUID                   │
  │     B checks for duplicate connection    │
  │     B merges A's peer list               │
  │                                          │
  │◄─── PEER_EXCHANGE_RESPONSE ────────────│
  │     { uuid: B, port: 12346,             │
  │       peers: [{10.0.0.3:12346}, ...] }  │
  │                                          │
  │     A records B's UUID                   │
  │     A checks for duplicate connection    │
  │     A merges B's peer list               │
  │                                          │
  │──── (normal P2P traffic: BLOCK, SYNC)   │
```

### Periodic Exchange

```
Node A                                    Node B
  │                                          │
  │  (exchange timer fires)                  │
  │──── PEER_EXCHANGE ─────────────────────►│
  │     { uuid: A, port: 12346,             │
  │       peers: [current full list] }       │
  │                                          │
  │◄─── PEER_EXCHANGE_RESPONSE ────────────│
  │     { uuid: B, port: 12346,             │
  │       peers: [current full list] }       │
  │                                          │
```

### Duplicate Connection Detection

```
Node A (uuid: "aaa...")                  Node B (uuid: "bbb...")
  │                                          │
  │──── outbound connect ──────────────────►│  (A has outbound to B)
  │◄─── PEER_EXCHANGE_RESPONSE ────────────│
  │                                          │
  │  Meanwhile, B also discovers A:          │
  │◄─── outbound connect ─────────────────│  (B has outbound to A)
  │──── PEER_EXCHANGE_RESPONSE ────────────►│
  │                                          │
  │  Both detect duplicate (same UUID pair): │
  │  Compare UUIDs: "aaa..." < "bbb..."      │
  │  → A keeps its outbound to B             │
  │  → B drops its outbound to A             │
  │                                          │
  │  Result: single connection (A→B)         │
```

## Processing Rules

### On Receiving PEER_EXCHANGE or PEER_EXCHANGE_RESPONSE

1. **Record sender identity**: Store the sender's UUID and listen port, associating them with this connection.

2. **Duplicate detection**: If a connection to the same UUID already exists on a different socket:
   - Compare UUIDs of the two endpoints (local and remote).
   - The node with the **lower UUID** keeps its **outbound** connection.
   - The other node drops its outbound connection.
   - If both connections are inbound (shouldn't happen with proper dedup), drop the newer one.

3. **Self-filter**: Discard any peer entry whose address matches the local node's listen address, or whose UUID matches the local node's UUID.

4. **Ban check**: Discard any peer entry whose address appears in the local ban list.

5. **Merge peers**: For each remaining peer entry in the received list:
   - If not already known: add to peer list (if under `max_stored_peers` cap; evict oldest-seen if at cap).
   - If already known: update `last_seen` timestamp.

6. **Connect to new peers**: If `discovery_enabled` and outbound connection count is below `max_outbound`, attempt connections to newly discovered peers.

7. **Persist**: Write updated `peers.json` atomically.

### On Receiving PEER_EXCHANGE (Inbound Connection)

Additionally, after processing, send a `PEER_EXCHANGE_RESPONSE` back.

## Inbound Connection Limit Enforcement

Before accepting a TLS handshake on the P2P acceptor, the `Server` checks `PeerManager::can_accept_inbound()`. If the inbound connection count equals `max_inbound`, the socket is closed immediately without handshake.

## Ban Enforcement

After TLS handshake completes on an inbound connection, `PeerServer` checks the remote address against the ban list via `PeerManager::is_banned(host, port)`. If banned, the connection is closed without sending any messages.

## Compatibility

- Nodes running pre-004 code do not recognize `PEER_EXCHANGE` (type 3) or `PEER_EXCHANGE_RESPONSE` (type 4). The existing `PeerServer::do_read_body` logs unknown packet types and continues reading. Pre-004 nodes will ignore peer exchange messages and continue functioning for block and sync traffic.
- 004 nodes connecting to pre-004 nodes will send `PEER_EXCHANGE` after handshake. The pre-004 node will log an unknown packet type and not respond. The 004 node will not receive a `PEER_EXCHANGE_RESPONSE` and will treat the peer as having an unknown UUID (empty string). Duplicate detection will fall back to address-based comparison for such peers.
