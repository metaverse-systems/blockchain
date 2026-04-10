# Data Model: Peer Discovery & Management

**Feature**: 004-peer-discovery
**Date**: 2026-04-10

## New Entities

### NodeConfig

Unified node configuration loaded from `config.json`. Replaces `.env` / `loadDotEnv`.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| tls.cert_file | `string` | `"cert.pem"` | Path to TLS certificate (relative to data dir or absolute) |
| tls.key_file | `string` | `"key.pem"` | Path to TLS private key |
| tls.ca_file | `string` | `""` | Path to CA bundle for peer verification (empty = system default) |
| network.rpc_port | `uint16_t` | `12345` | JSON-RPC listen port |
| network.p2p_port | `uint16_t` | `12346` | P2P binary protocol listen port |
| network.timeout_seconds | `uint32_t` | `30` | Connection timeout for TLS handshake |
| consensus.* | various | (see ConsensusConfig) | All existing consensus parameters, migrated from env |
| peers.seed_nodes | `vector<{host, port}>` | `[]` | Initial peers to connect to on startup |
| peers.max_outbound | `uint16_t` | `8` | Maximum outbound peer connections |
| peers.max_inbound | `uint16_t` | `32` | Maximum inbound peer connections |
| peers.exchange_interval_seconds | `uint32_t` | `30` | Seconds between periodic peer exchanges |
| peers.discovery_enabled | `bool` | `true` | Enable automatic peer discovery; false = manual RPC only |
| peers.max_stored_peers | `uint16_t` | `256` | Maximum peer entries in peers.json |
| peers.reconnect_base_delay_seconds | `uint32_t` | `5` | Initial reconnection delay |
| peers.reconnect_max_delay_seconds | `uint32_t` | `300` | Maximum reconnection delay (cap for exponential backoff) |
| peers.ban_threshold_errors | `uint32_t` | `10` | Error count before automatic ban |
| peers.ban_duration_seconds | `uint32_t` | `3600` | Duration of automatic ban in seconds |

**Persistence**: `config.json` in the blockchain data directory. Operator-managed. Read-only by the daemon after startup.

**Validation rules**:
- `tls.cert_file` and `tls.key_file` must be non-empty
- Ports must be > 0 and ≤ 65535
- `rpc_port` and `p2p_port` must differ
- `max_outbound` and `max_inbound` must be > 0
- `exchange_interval_seconds` must be ≥ 5
- `reconnect_base_delay_seconds` must be ≥ 1
- `reconnect_max_delay_seconds` must be ≥ `reconnect_base_delay_seconds`
- `ban_threshold_errors` must be ≥ 1

### PeerConfig

Subset of NodeConfig containing only peer discovery settings. Extracted for passing to `PeerManager` without coupling it to the full config.

| Field | Type | Description |
|-------|------|-------------|
| seed_nodes | `vector<PeerAddress>` | Seed node addresses |
| max_outbound | `uint16_t` | Max outbound connections |
| max_inbound | `uint16_t` | Max inbound connections |
| exchange_interval_seconds | `uint32_t` | Peer exchange timer interval |
| discovery_enabled | `bool` | Automatic discovery toggle |
| max_stored_peers | `uint16_t` | Peer storage cap |
| reconnect_base_delay_seconds | `uint32_t` | Base reconnect delay |
| reconnect_max_delay_seconds | `uint32_t` | Max reconnect delay |
| ban_threshold_errors | `uint32_t` | Error threshold for ban |
| ban_duration_seconds | `uint32_t` | Auto-ban duration |

### PeerAddress

Represents a network endpoint for a peer.

| Field | Type | Description |
|-------|------|-------------|
| host | `string` | IPv4 or IPv6 address |
| port | `uint16_t` | P2P listen port |

**Serialization**: Boost.Serialization (for P2P wire) and nlohmann/json (for peers.json).

### PeerEntry

A known peer with associated runtime state. Stored in `peers.json`.

| Field | Type | Description |
|-------|------|-------------|
| host | `string` | IPv4 or IPv6 address |
| port | `uint16_t` | P2P listen port |
| node_uuid | `string` | UUID of the peer (empty if not yet exchanged) |
| last_seen | `uint64_t` | Unix timestamp of last successful communication |
| error_count | `uint32_t` | Number of consecutive errors from this peer |

**Validation rules**:
- `host` must be non-empty
- `port` must be > 0
- `node_uuid` may be empty (filled on first successful exchange)
- `last_seen` = 0 for never-contacted peers

### BanRecord

Tracks a currently banned peer.

| Field | Type | Description |
|-------|------|-------------|
| host | `string` | Banned peer's address |
| port | `uint16_t` | Banned peer's port |
| reason | `string` | Ban reason (`"excessive_errors"`, `"manual"`, etc.) |
| expires | `uint64_t` | Unix timestamp when ban expires (0 = permanent until unban) |

**State transitions**:
- Peer error_count exceeds `ban_threshold_errors` → create BanRecord, remove from active peers
- Ban expires (current time > `expires`) → remove BanRecord, peer eligible for rediscovery
- Operator issues `unbanPeer` RPC → remove BanRecord immediately

### PeerExchangeRequest

P2P message sent to initiate or periodically refresh peer exchange.

| Field | Type | Description |
|-------|------|-------------|
| sender_uuid | `string` | UUID of the sending node |
| sender_listen_port | `uint16_t` | P2P listen port of the sender |
| peers | `vector<PeerAddress>` | Full list of non-banned known peer addresses |

**Serialization**: Boost.Serialization binary archive.
**Wire identification**: `PacketType::PEER_EXCHANGE` (new).

### PeerExchangeResponse

P2P message sent in reply to a `PeerExchangeRequest`.

| Field | Type | Description |
|-------|------|-------------|
| sender_uuid | `string` | UUID of the responding node |
| sender_listen_port | `uint16_t` | P2P listen port of the responder |
| peers | `vector<PeerAddress>` | Full list of non-banned known peer addresses |

**Serialization**: Boost.Serialization binary archive.
**Wire identification**: `PacketType::PEER_EXCHANGE_RESPONSE` (new).

### PeerManager

Central class managing peer lifecycle. Not a data entity but owns and coordinates all peer state.

| Responsibility | Description |
|---------------|-------------|
| Peer list | Maintains `vector<PeerEntry>` capped at `max_stored_peers`, evicts oldest-seen first |
| Node UUID | Loads from `peers.json` or generates on first run |
| Outbound connections | Manages `PeerClient` instances up to `max_outbound` |
| Inbound tracking | Tracks inbound connection count, rejects when at `max_inbound` |
| Exchange timer | Fires every `exchange_interval_seconds`, triggers peer exchange on all connections |
| Reconnection | Schedules reconnection with exponential backoff per disconnected peer |
| Ban enforcement | Checks ban list before accepting connections or initiating outbound |
| Persistence | Writes `peers.json` atomically after peer list changes |
| Duplicate detection | Compares node UUIDs; lower UUID keeps outbound on conflict |
| Self-filtering | Discards own address/UUID from received peer lists |

## Modified Entities

### PacketType (enum in PacketHeader.hpp)

| Value | Name | Status |
|-------|------|--------|
| 0 | `BLOCK` | Existing |
| 1 | `BLOCKCHAIN_QUERY` | Existing |
| 2 | `BLOCKCHAIN_RESPONSE` | Existing |
| 3 | `PEER_EXCHANGE` | **New** |
| 4 | `PEER_EXCHANGE_RESPONSE` | **New** |

### PeerClient

| Change | Description |
|--------|-------------|
| Owned by PeerManager | PeerManager creates and owns PeerClient instances (currently main.cpp creates one) |
| Reconnection backoff | On disconnect, PeerManager schedules reconnection with backoff timer |
| UUID exchange | Sends PEER_EXCHANGE immediately after TLS handshake (before sync) |
| Periodic exchange | PeerManager triggers exchange on all connected PeerClients |

### PeerServer

| Change | Description |
|--------|-------------|
| Inbound limit check | Checks PeerManager before accepting TLS handshake |
| Handle PEER_EXCHANGE | Deserializes, updates PeerManager, sends PEER_EXCHANGE_RESPONSE |
| Handle PEER_EXCHANGE_RESPONSE | Deserializes, updates PeerManager |
| Ban check | Rejects connections from banned addresses |

### RpcServer

| New Method | Description |
|------------|-------------|
| `addPeer` | Adds a peer address and initiates connection |
| `removePeer` | Disconnects and removes a peer |
| `listPeers` | Returns current peer list with status |
| `banPeer` | Manually bans a peer address |
| `unbanPeer` | Removes a ban |

### main.cpp

| Change | Description |
|--------|-------------|
| Load `config.json` via NodeConfig | Replaces `.env` loading and individual `getenv()` calls |
| Create PeerManager | Owns peer lifecycle, passed to Server/RpcServer |
| Remove hardcoded ports | Ports come from NodeConfig |
| Remove loadDotEnv call | Replaced by NodeConfig |

### utils.hpp / utils.cpp

| Change | Description |
|--------|-------------|
| Remove `loadDotEnv` | No longer needed; config.json replaces .env |

## State Transitions

```
Node startup
  │
  ├─ Load config.json → NodeConfig
  ├─ Load peers.json → PeerManager (or generate UUID if first run)
  ├─ Purge expired bans
  │
  ├─ For each seed node (if discovery_enabled):
  │    └─ PeerManager::connect_to(seed) → PeerClient created
  │
  ├─ For each persisted peer (up to max_outbound - seed count):
  │    └─ PeerManager::connect_to(peer) → PeerClient created
  │
  └─ Start exchange timer
       │
       Every exchange_interval_seconds:
       │  For each connected peer:
       │    └─ Send PEER_EXCHANGE
       │
       On PEER_EXCHANGE received (inbound):
       │  ├─ Record sender UUID
       │  ├─ Check for duplicate connection (UUID match)
       │  ├─ Merge received peers into local list (cap at max_stored_peers)
       │  ├─ Filter self-address
       │  ├─ Attempt connections to new peers (if under max_outbound)
       │  └─ Send PEER_EXCHANGE_RESPONSE
       │
       On peer disconnect:
       │  ├─ Update peer entry (increment error_count if unexpected)
       │  ├─ If error_count >= ban_threshold → ban peer
       │  └─ Schedule reconnection with backoff (if not banned)
       │
       On shutdown:
            └─ Save peers.json atomically
```

## Entity Relationship Diagram

```
NodeConfig ──────────────── contains ──► PeerConfig
                                             │
                                             │ configures
                                             ▼
                                        PeerManager
                                        ┌────┴────┐
                            owns/manages│         │owns/manages
                                        ▼         ▼
                                  PeerEntry    BanRecord
                                  (peers.json) (peers.json)
                                        │
                                        │ connects via
                                        ▼
                              PeerClient / PeerServer
                                        │
                                        │ exchanges
                                        ▼
                            PeerExchangeRequest/Response
                                 (P2P wire format)
```
