# Monitoring, Metrics & Health Endpoint

Feature 023 adds HTTPS monitoring endpoints and JSON structured logging to the blockchain node.

## Endpoints

The monitoring server listens on a separate HTTPS port (default 9090) using the same TLS certificates as the RPC server.

### GET /health

Returns a JSON health check response with the current node status:

```bash
curl -k https://localhost:9090/health
```

```json
{
  "status": "ok",
  "chain_height": 42,
  "peer_count": 3,
  "chunk_count": 1,
  "uptime_seconds": 1234.56,
  "last_block_index": 41
}
```

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | `"ok"` or `"shutting_down"` |
| `chain_height` | integer | Current number of blocks in the chain |
| `peer_count` | integer | Active inbound + outbound peer connections |
| `chunk_count` | integer | Number of chunk files on disk |
| `uptime_seconds` | float | Seconds since node startup |
| `last_block_index` | integer | Index of the last block (chain_height - 1, or -1 if empty) |

### GET /metrics

Returns Prometheus-compatible metrics in Exposition Format v0.0.4:

```bash
curl -k https://localhost:9090/metrics
```

```
# HELP blockchain_chain_height Current chain height
# TYPE blockchain_chain_height gauge
blockchain_chain_height 42

# HELP blockchain_peer_count Active peer connections
# TYPE blockchain_peer_count gauge
blockchain_peer_count 3

# HELP blockchain_chunk_count Number of chunk files
# TYPE blockchain_chunk_count gauge
blockchain_chunk_count 1

# HELP blockchain_active_connections Active inbound + outbound connections
# TYPE blockchain_active_connections gauge
blockchain_active_connections 3

# HELP blockchain_uptime_seconds Node uptime in seconds
# TYPE blockchain_uptime_seconds gauge
blockchain_uptime_seconds 1234.56

# HELP blockchain_rpc_requests_total Total RPC requests received
# TYPE blockchain_rpc_requests_total counter
blockchain_rpc_requests_total 150

# HELP blockchain_rpc_errors_total Total RPC errors returned
# TYPE blockchain_rpc_errors_total counter
blockchain_rpc_errors_total 3

# HELP blockchain_blocks_received_total Total blocks validated and appended
# TYPE blockchain_blocks_received_total counter
blockchain_blocks_received_total 42

# HELP blockchain_blocks_rejected_total Total blocks rejected (invalid or rate-limited)
# TYPE blockchain_blocks_rejected_total counter
blockchain_blocks_rejected_total 1
```

## Configuration

Monitoring is configured via `config.json` in the blockchain data directory:

```json
{
  "network": {
    "monitoring_enabled": true,
    "monitoring_port": 9090,
    "monitoring_bind_address": "127.0.0.1",
    "log_format": "text"
  }
}
```

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `monitoring_enabled` | boolean | `false` | Enable/disable monitoring server |
| `monitoring_port` | integer | `9090` | Port for HTTPS monitoring endpoints |
| `monitoring_bind_address` | string | `"127.0.0.1"` | Bind address (IPv4 or IPv6) |
| `log_format` | string | `"text"` | Log format: `"text"` (colored stderr) or `"json"` (JSON Lines) |

### CLI Override

The monitoring port can be overridden via command line:

```bash
./blockchain --monitoring-port 9091 ./bc-dir/
```

## JSON Structured Logging

When `log_format` is set to `"json"`, all log output is emitted as JSON Lines to stderr:

```json
{"timestamp":"2026-06-23T10:30:00Z","level":"INFO","message":"Monitoring HTTPS server starting"}
{"timestamp":"2026-06-23T10:30:01Z","level":"DEBUG","message":"Connected to peer 127.0.0.1:12345"}
```

| Field | Type | Description |
|-------|------|-------------|
| `timestamp` | string | ISO 8601 UTC timestamp |
| `level` | string | Log level: `TRACE`, `DEBUG`, `INFO`, `WARNING`, `ERROR` |
| `message` | string | Log message (JSON-escaped) |

## Architecture

### MetricsCollector

The `MetricsCollector` class holds atomic counters and computes gauges from live component state:

- **Counters** (monotonic, atomic): `rpc_requests_total`, `rpc_errors_total`, `blocks_received_total`, `blocks_rejected_total`
- **Gauges** (computed on scrape): `chain_height`, `peer_count`, `chunk_count`, `active_connections`, `uptime_seconds`

### MonitoringHttpServer

A lightweight HTTPS server built on Boost.Asio SSL that:

1. Accepts incoming TLS connections
2. Performs TLS handshake
3. Reads HTTP request line via `async_read_until`
4. Routes `/health` and `/metrics` to JSON and Prometheus text handlers
5. Returns 404 for unknown paths
6. Closes connection after response

### Metrics Hooks

Counters are incremented at the source:

- **RPC requests/errors**: Injected in `RpcServer::do_read()` dispatch loop — every JSON-RPC request increments `rpc_requests_total`, responses with an `"error"` key increment `rpc_errors_total`
- **Blocks received**: Incremented in `BlockPropagation::on_block_received()` when a block is validated and appended
- **Blocks rejected**: Incremented in `BlockPropagation::on_block_received()` when a block fails consensus validation or exceeds rate limits
- **Peer counts**: Computed live from `PeerManager::outbound_count() + inbound_count()` (no dedicated counter)

### Wiring in main.cpp

On startup (when `monitoring_enabled` is true):

1. `MetricsCollector` is created and given references to `Blockchain` and `PeerManager`
2. `PeerManager::set_metrics_collector()` is called
3. `BlockPropagation::set_metrics_collector()` is called
4. `Server::set_metrics_collector()` propagates to new `RpcServer` sessions
5. `MonitoringHttpServer` is created with the shared SSL context and started

## Security Considerations

- The monitoring server uses HTTPS (TLS) with the same certificates as the RPC server
- Default bind address is `127.0.0.1` (localhost only)
- The server is disabled by default (`monitoring_enabled: false`)
- No authentication is currently implemented — bind to localhost or place behind a reverse proxy
