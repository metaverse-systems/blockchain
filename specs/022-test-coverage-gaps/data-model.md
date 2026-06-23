# Data Model: Peer Disconnect Test Coverage

This feature adds test coverage only — no new production data models are introduced. The existing models exercised by tests are documented below for reference.

## Existing Entities (exercised by tests)

### BlockPropagation state
| Field | Type | Description |
|-------|------|-------------|
| `dedup_set_` | `std::unordered_set<std::string>` | Recent block hash cache (max 512) |
| `pending_map_` | `std::unordered_map<std::string, PendingBlock>` | Gap block pool (max 64, 60s TTL) |
| `sync_queue_` | `std::deque<pair<Block, string>>` | Blocks queued during sync (max 128) |
| `rate_states_` | `std::unordered_map<std::string, BlockRateState>` | Per-peer rate limiter |
| `relay_cb_` | `std::function<void(Block&, string&)>` | Callback to relay block to peers |

### PeerManager state
| Field | Type | Description |
|-------|------|-------------|
| `peers_` | `std::unordered_map<string, PeerEntry>` | Known peer entries keyed by `host:port` |
| `outbound_connections_` | `std::unordered_map<string, shared_ptr<PeerClient>>` | Active outbound connections |
| `inbound_sessions_` | `std::unordered_map<string, weak_ptr<PeerServer>>` | Active inbound sessions |
| `bans_` | `std::unordered_map<string, BanRecord>` | Ban records |
| `backoff_state_` | `std::unordered_map<string, BackoffEntry>` | Reconnection backoff state |
| `inbound_count_` | `size_t` | Current inbound connection count |

### PeerEntry state
| Field | Type | Description |
|-------|------|-------------|
| `host` | `std::string` | Peer IP/hostname |
| `port` | `uint16_t` | Peer port |
| `node_uuid` | `std::string` | Remote node UUID |
| `last_seen` | `uint64_t` | Unix timestamp of last activity |
| `error_count` | `uint32_t` | Cumulative error count (incremented on disconnect) |

## State Transitions

### Outbound Disconnect (`on_peer_disconnected`)
```
Before: peer in outbound_connections_, error_count = N, not banned
After:  peer removed from outbound_connections_, error_count = N+1
        → schedule_reconnect() if: not banned AND no inbound session from same UUID
```

### Inbound Disconnect (`on_inbound_disconnected`)
```
Before: session in inbound_sessions_, inbound_count_ = M
After:  session removed from inbound_sessions_, inbound_count_ = M-1
```

### Relay Exception During Propagation
```
Before: block validated, relay_cb_ invoked with (block, sender_key)
During: relay_cb_ may throw exception for one or more peers
After:  block still appended (append happens before relay)
        exception caught, propagation continues normally
```

## Validation Rules

- `error_count` is unsigned (uint32_t) — no overflow concern in test scenarios
- `inbound_count_` decrements with guard: `if (inbound_count_ > 0) inbound_count_--`
- Banned peers never get reconnection scheduled
- Duplicate connection detection (same UUID, inbound + outbound) prevents unnecessary reconnects
