# Data Model: Monitoring, Metrics & Health Endpoint

## Entities

### MetricsCollector

Central class that maintains atomic counter/gauge values. Not persisted — in-memory only.

| Field | Type | Description |
|-------|------|-------------|
| `rpc_requests_total_` | `std::atomic<uint64_t>` | Total RPC requests processed (counter, monotonic) |
| `rpc_errors_total_` | `std::atomic<uint64_t>` | Total RPC errors returned (counter, monotonic) |
| `blocks_received_total_` | `std::atomic<uint64_t>` | Total blocks received from P2P (counter, monotonic) |
| `blocks_rejected_total_` | `std::atomic<uint64_t>` | Total blocks rejected by validation (counter, monotonic) |
| `start_time_` | `std::chrono::steady_clock::time_point` | Node start time (set at construction) |

**Derived values** (not stored, computed on scrape):
- `chain_height` — from `Blockchain::chainHeight()`
- `peer_count` — from `PeerManager::get_peers().size()`
- `chunk_count` — from `Blockchain::getChunkCount()`
- `uptime_seconds` — `steady_clock::now() - start_time_`
- `active_connections` — from `PeerManager` connection count

**Thread safety**: All counters are `std::atomic<uint64_t>` — lock-free increments. Gauges are computed from mutex-protected component state on scrape.

---

### HealthResponse

JSON structure returned by `/health` endpoint. Not persisted.

| Field | Type | Description |
|-------|------|-------------|
| `status` | `std::string` | `"healthy"` or `"shutting_down"` |
| `chain_height` | `int64_t` | Current chain height (0 if empty) |
| `peer_count` | `int64_t` | Number of connected peers |
| `chunk_count` | `int64_t` | Number of chunks on disk |
| `uptime_seconds` | `double` | Seconds since node start |
| `last_block_index` | `int64_t` | Index of last block (-1 if empty) |

**Validation**: All fields are always present. `status` is derived from `IBlockchain::isShuttingDown()`.

---

### PrometheusMetric

Internal representation of a single Prometheus metric line. Not persisted.

| Field | Type | Description |
|-------|------|-------------|
| `name` | `std::string` | Metric name (e.g., `blockchain_chain_height`) |
| `help` | `std::string` | HELP text |
| `type` | `std::string` | `gauge` or `counter` |
| `value` | `double` | Current value |
| `labels` | `std::map<std::string, std::string>` | Optional label key-value pairs |

**Format rules**:
- Names must match `[a-zA-Z_][a-zA-Z0-9_]*`
- Values are floating point (Go `ParseFloat` compatible)
- Labels are optional curly-brace key-value pairs

---

### MonitoringConfig

Configuration fields added to `NodeConfig::NetworkConfig`. Persisted in `config.json`.

| Field | Type | Default | Validation |
|-------|------|---------|------------|
| `monitoring_enabled` | `bool` | `false` | — |
| `monitoring_port` | `uint16_t` | `9090` | Must be > 0 and != rpc_port, p2p_port |
| `monitoring_bind_address` | `std::string` | `"127.0.0.1"` | Valid IPv4 or IPv6 address (e.g., `127.0.0.1`, `0.0.0.0`, `::1`, `::`) |
| `log_format` | `std::string` | `"text"` | Must be `"text"` or `"json"` |

---

### LogLevel (Extended)

Enum extended with Trace level. Not persisted (value stored in `config.json` as string).

| Value | Enum | Description |
|-------|------|-------------|
| 0 | `Trace` | Verbose diagnostic output (new) |
| 1 | `Debug` | Debug output |
| 2 | `Info` | Informational messages |
| 3 | `Warning` | Warning messages |
| 4 | `Error` | Error messages |

**Filtering**: Level N shows messages with level >= N. Configuring `"warn"` shows Warning and Error.

---

## State Transitions

### Node Monitoring Lifecycle

```
[Disabled] -- monitoring_enabled: true --> [Starting]
[Starting] -- ssl context loaded, acceptor bind OK --> [Running]
[Starting] -- ssl context load FAIL or bind FAIL --> [Disabled] (log error, continue)
[Running] -- SIGINT/SIGTERM --> [Stopping]
[Stopping] -- cancel acceptor, close TLS connections, shutdown context --> [Stopped]
```

### Health Status

```
[healthy] -- isShuttingDown() == true --> [shutting_down]
```

### Log Format

```
[text] -- log_format: "json" --> [json]
[json] -- log_format: "text" --> [text]
```

Hot-reload: Format change takes effect on next log message (no restart needed).
