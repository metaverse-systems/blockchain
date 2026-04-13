# Data Model: Performance & Deduplication Cleanup

**Date**: 2026-04-13  
**Feature**: 019-perf-dedup-cleanup  

## Modified Entities

### PeerEntry (existing — container change only)

No field changes. The storage container changes from `std::vector<PeerEntry>` to `std::unordered_map<std::string, PeerEntry>` keyed by `peer_key(host, port)`.

| Field | Type | Description |
|-------|------|-------------|
| host | string | Peer hostname/IP (normalized) |
| port | uint16_t | Peer listen port |
| node_uuid | string | Remote node UUID |
| last_seen | uint64_t | Unix timestamp of last contact |
| error_count | uint32_t | Consecutive error count |

**Key**: `peer_key(host, port)` → `"host:port"` string
**Serialization**: JSON (to/from `peers.json`) — unchanged format
**Validation**: Host is normalized via `normalize_address()` before key construction

### BanRecord (existing — container change only)

No field changes. The storage container changes from `std::vector<BanRecord>` to `std::unordered_map<std::string, BanRecord>` keyed by `peer_key(host, port)`.

| Field | Type | Description |
|-------|------|-------------|
| host | string | Banned peer hostname/IP |
| port | uint16_t | Banned peer port |
| reason | string | Ban reason |
| expires | uint64_t | Unix timestamp; 0 = permanent |

**Key**: `peer_key(host, port)` → `"host:port"` string
**Serialization**: JSON (to/from `peers.json`) — unchanged format
**State transition**: `expires == 0` → permanent ban; `expires > 0 && expires <= now` → expired (removed by `purge_expired_bans()`)

## New Entities

### RPC Dispatch Table

A mapping from JSON-RPC method names to handler functions, stored as a private member of `RpcServer`.

| Field | Type | Description |
|-------|------|-------------|
| method_name | string (key) | JSON-RPC method name (e.g., "publish") |
| handler | function | Callable returning JSON response |

**Type**: `std::unordered_map<std::string, std::function<nlohmann::json(const nlohmann::json&)>>`
**Lifecycle**: Initialized once in `RpcServer` constructor; immutable thereafter
**Cardinality**: Exactly 20 entries (one per existing RPC method)

### PacketSerializer (utility — no persistent state)

A header-only template function that serializes an object with Boost.Serialization and prepends a `PacketHeader`. No stored state — pure function.

**Input**: Serializable object of type T, packet type enum value
**Output**: Pair of (header bytes, serialized payload string)
**Wire format**: `[PacketHeader: 16 bytes][serialized payload: N bytes]` — identical to current format

## Relationships

```
PeerManager
  ├── peers_: unordered_map<string, PeerEntry>   (was: vector<PeerEntry>)
  ├── bans_: unordered_map<string, BanRecord>     (was: vector<BanRecord>)
  └── peer_key(host, port) → string key            (existing static helper)

RpcServer
  └── dispatch_: unordered_map<string, RpcHandler> (new)
       └── 20 handler methods (extracted from do_read)

PeerClient::send<T>()  ──uses──▶  serialize_packet<T>()  (PacketSerializer.hpp)
PeerServer::send_packet<T>()  ──uses──▶  serialize_packet<T>()  (PacketSerializer.hpp)
```

## On-Disk Format

No changes. `peers.json` continues to store peers as a JSON array and bans as a JSON array. The internal container type is transparent to the serialized format.
