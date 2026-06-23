# Contract: /metrics HTTPS Endpoint

## Overview

TLS-protected HTTPS metrics endpoint. Returns Prometheus-compatible text-format time-series data for scraping.

## Request

```
GET /metrics HTTP/1.1
Host: localhost:9090
```

Connection is TLS-encrypted. No additional authentication. No request body, no query parameters.

## Response

```
HTTP/1.1 200 OK
Content-Type: text/plain; version=0.0.4
Content-Length: 512

# HELP blockchain_chain_height Current blockchain height.
# TYPE blockchain_chain_height gauge
blockchain_chain_height 1234

# HELP blockchain_peer_count Number of currently connected peers.
# TYPE blockchain_peer_count gauge
blockchain_peer_count 5

# HELP blockchain_chunk_count Number of chunks on disk.
# TYPE blockchain_chunk_count gauge
blockchain_chunk_count 13

# HELP blockchain_active_connections Number of active TCP connections.
# TYPE blockchain_active_connections gauge
blockchain_active_connections 5

# HELP blockchain_uptime_seconds Seconds since node start.
# TYPE blockchain_uptime_seconds gauge
blockchain_uptime_seconds 3600.5

# HELP blockchain_rpc_requests_total Total RPC requests processed.
# TYPE blockchain_rpc_requests_total counter
blockchain_rpc_requests_total 150

# HELP blockchain_rpc_errors_total Total RPC errors returned.
# TYPE blockchain_rpc_errors_total counter
blockchain_rpc_errors_total 3

# HELP blockchain_blocks_received_total Total blocks received from P2P.
# TYPE blockchain_blocks_received_total counter
blockchain_blocks_received_total 200

# HELP blockchain_blocks_rejected_total Total blocks rejected by validation.
# TYPE blockchain_blocks_rejected_total counter
blockchain_blocks_rejected_total 5
```

## Metric Definitions

### Gauges (current state, can go up or down)

| Metric | Type | Description |
|--------|------|-------------|
| `blockchain_chain_height` | gauge | Current blockchain height (0 for empty chain) |
| `blockchain_peer_count` | gauge | Currently connected peers |
| `blockchain_chunk_count` | gauge | Chunks persisted on disk |
| `blockchain_active_connections` | gauge | Active TCP connections |
| `blockchain_uptime_seconds` | gauge | Seconds since node start (monotonically increasing gauge) |

### Counters (monotonically increasing)

| Metric | Type | Description |
|--------|------|-------------|
| `blockchain_rpc_requests_total` | counter | Total RPC requests since node start |
| `blockchain_rpc_errors_total` | counter | Total RPC errors since node start |
| `blockchain_blocks_received_total` | counter | Total blocks received from P2P since node start |
| `blockchain_blocks_rejected_total` | counter | Total blocks rejected by validation since node start |

## Format Compliance

Follows Prometheus text exposition format v0.0.4:
- UTF-8 encoded, `\n` line endings
- `# HELP` and `# TYPE` comments before each metric
- One metric per line: `metric_name value`
- No labels used in current design (reserved for future per-peer or per-stream metrics)

## Performance

- Response time: <50ms under normal load
- Counters: lock-free `std::atomic<uint64_t>` reads
- Gauges: computed on-scrape from component state (may block briefly on mutex)
- Staleness: <5 seconds (gauges reflect state at scrape time)
