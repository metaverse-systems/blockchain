# Research: Monitoring, Metrics & Health Endpoint

## Decision: Prometheus Exposition Format

**Rationale**: Industry standard for metrics scraping, compatible with Prometheus/Grafana/Loki stack.

**Format**:
- Content-Type: `text/plain; version=0.0.4`
- UTF-8, `\n` line endings, last line must end with `\n`
- `# HELP` and `# TYPE` comments before metric samples
- Metric line: `metric_name{labels} value`

**Examples for this project**:
```
# HELP blockchain_chain_height Current blockchain height.
# TYPE blockchain_chain_height gauge
blockchain_chain_height 1234

# HELP blockchain_rpc_requests_total Total RPC requests processed.
# TYPE blockchain_rpc_requests_total counter
blockchain_rpc_requests_total 150

# HELP blockchain_peer_count Number of currently connected peers.
# TYPE blockchain_peer_count gauge
blockchain_peer_count 5
```

**Alternatives considered**: JSON metrics endpoint (rejected — Prometheus text format is the standard), OpenMetrics (rejected — overkill for current needs)

---

## Decision: TLS Boost.Asio HTTPS Server

**Rationale**: Monitoring server must comply with constitution's Mandatory TLS principle (§VI). Reuses existing Boost.Asio SSL infrastructure from RpcServer/PeerServer.

**Pattern**:
- Standalone `MonitoringHttpServer` class (not derived from `SessionHandler`)
- `ssl::stream<tcp::socket>` (TLS via `ssl::context`, shared with RpcServer)
- `async_read_until` for HTTP request line parsing (after TLS handshake)
- Synchronous GET-only responses
- Separate port from JSON-RPC server (default 9090)
- IPv4/IPv6 support via `boost::asio::ip::tcp::v4()` / `boost::asio::ip::tcp::v6()` based on bind address
- Bind address configurable via `network.monitoring_bind_address` in `config.json` (default `127.0.0.1`)

**Architecture**:
```
MonitoringHttpServer
├── ssl::acceptor (binds to configured address:port, default 127.0.0.1:9090)
├── ssl::context (shared with RpcServer, uses same cert/key)
├── /health → returns HealthResponse JSON
├── /metrics → returns Prometheus text format
└── 404 → for unknown paths
```

**IPv6 Support**: Accept IPv4 (`127.0.0.1`, `0.0.0.0`) or IPv6 (`::1`, `::`) bind addresses. `tcp::acceptor` resolves address family from the bind address string. Dual-stack is available where the OS supports it.

**Certificate Management**: Reuses existing TLS cert/key from `config.json` (`network.tls.cert_file`, `network.tls.key_file`). Operators can use the same cert as the RPC server or provide a separate one.

**Alternatives considered**: Plain TCP (rejected — violates constitution §VI), separate process (rejected — too heavy)

---

## Decision: MetricsCollector with Atomic Counters

**Rationale**: Lock-free, zero-allocation hooks. Raw pointer stored in each component. Single-atomic-increment at each event point. No polling timers.

**MetricsCollector design**:
```cpp
class MetricsCollector {
    std::atomic<uint64_t> rpc_requests_total_{0};
    std::atomic<uint64_t> rpc_errors_total_{0};
    std::atomic<uint64_t> blocks_received_total_{0};
    std::atomic<uint64_t> blocks_rejected_total_{0};
    // ... gauges are refreshed on scrape (not atomic)
};
```

**Hook injection pattern**: Raw pointer `MetricsCollector* metrics_` member in PeerManager, RpcServer, Blockchain, BlockPropagation. Set during construction. Each hook: `if (metrics_) metrics_->rpc_requests_total_++;`

**Alternatives considered**: Callback hooks with `std::function<void()>` (rejected — virtual call overhead, exception risk), polling timers (rejected — spec explicitly asks for callbacks not polling), shared_ptr (rejected — raw pointer sufficient, owned by MonitoringHttpServer)

---

## Decision: LogLevel Trace + JSON Structured Logging

**Rationale**: Extends existing log infrastructure without breaking changes. JSON output uses nlohmann/json (already vendored).

**Changes to LogLevel enum**:
```cpp
// Current:
enum class LogLevel { Debug = 0, Info = 1, Warning = 2, Error = 3 };

// New:
enum class LogLevel { Trace = 0, Debug = 1, Info = 2, Warning = 3, Error = 4 };
```

**Changes to logMessage()**:
- Detect `log_format` config ("text" or "json")
- Text format: current `[YYYY-MM-DD HH:MM:SS] [LEVEL] msg\n`
- JSON format: `{"timestamp":"...","level":"...","message":"..."}\n`

**New LOG_TRACE macro**: `#define LOG_TRACE(msg) do { if (getLogLevel() <= LogLevel::Trace) logMessage("TRACE", msg); } while(0)`

**Alternatives considered**: Separate JSON logger function (rejected — spec says replace logMessage), structured fields beyond level/message (rejected — out of scope for v1)

---

## Decision: HealthResponse Structure

**Rationale**: Simple JSON snapshot of node state, no database lookups.

```json
{
  "status": "healthy",
  "chain_height": 1234,
  "peer_count": 5,
  "chunk_count": 13,
  "uptime_seconds": 3600.5,
  "last_block_index": 1234
}
```

**Edge cases**:
- Empty chain (genesis only): `chain_height=0`, `last_block_index=-1`
- Shutdown: `status="shutting_down"` with current metrics
- No peers: `peer_count=0`, status still `"healthy"`

---

## Decision: Configuration Schema

**Rationale**: Reuses existing `config.json` pattern from spec 009.

```json
{
  "network": {
    "monitoring_enabled": false,
    "monitoring_port": 9090,
    "monitoring_bind_address": "127.0.0.1",
    "log_level": "info",
    "log_format": "text"
  }
}
```

Bind address accepts IPv4 (`127.0.0.1`, `0.0.0.0`) or IPv6 (`::1`, `::`). The `ssl::context` is shared with the RPC server, using the same cert/key files already configured in `network.tls`.

**CLI override**: `--monitoring-port` argument (added to CliParser)

**Alternatives considered**: Separate monitoring config file (rejected — adds complexity), environment variables (rejected — project uses config.json)

---

## Hook Insertion Points

### PeerManager
| Event | Location | Hook |
|-------|----------|------|
| Outbound connect success | PeerClient connect callback | `peer_connections_total++` |
| Outbound disconnect | `on_peer_disconnected()` | `peer_disconnections_total++` |
| Inbound connect | `on_inbound_connected()` | `peer_connections_total++` |
| Inbound disconnect | `on_inbound_disconnected()` | `peer_disconnections_total++` |
| Peer banned | `ban_peer()` | `peer_bans_total++` |
| Connection errors | `increment_error()` | `peer_errors_total++` |

### RpcServer
| Event | Location | Hook |
|-------|----------|------|
| RPC request received | `do_read()` after parse | `rpc_requests_total++` |
| RPC error response | `errorMessage()` returns | `rpc_errors_total++` |
| JSON parse error | `do_read()` catch | `rpc_errors_total++` |
| Unknown method | `do_read()` else branch | `rpc_errors_total++` |

### BlockPropagation
| Event | Location | Hook |
|-------|----------|------|
| Block received | `on_block_received()` | `blocks_received_total++` |
| Block rejected | `appendReceivedBlock()` catch | `blocks_rejected_total++` |

---

## Source File Plan

```
src/
├── MonitoringHttpServer.hpp   (new — TLS HTTPS server, IPv4/IPv6)
├── MonitoringHttpServer.cpp   (new — TLS handshake, GET-only, configurable bind)
├── MetricsCollector.hpp       (new)
├── MetricsCollector.cpp       (new)
├── utils.hpp                  (modify: LogLevel enum, logMessage, LOG_TRACE)
├── utils.cpp                  (modify: logMessage JSON output, parseLogLevel)
├── NodeConfig.hpp             (modify: NetworkConfig add monitoring fields)
├── NodeConfig.cpp             (modify: parse/generate monitoring config)
├── CliParser.hpp              (modify: add monitoring_port option)
├── CliParser.cpp              (modify: parse --monitoring-port)
├── main.cpp                   (modify: start monitoring server)
├── PeerManager.hpp            (modify: add MetricsCollector* member)
├── PeerManager.cpp            (modify: add hook points)
├── network/RpcServer.hpp      (modify: add MetricsCollector* member)
├── network/RpcServer.cpp      (modify: add hook points)
├── BlockPropagation.hpp       (modify: add MetricsCollector* member)
└── BlockPropagation.cpp       (modify: add hook points)

tests/
├── monitoring_tests.cpp       (new: unit tests)
└── monitoring_integration_tests.cpp  (new: HTTP endpoint tests)
```
