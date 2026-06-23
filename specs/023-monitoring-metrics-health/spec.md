# Feature Specification: Monitoring, Metrics & Health Endpoint

**Feature Branch**: `023-monitoring-metrics-health`  
**Created**: 2026-06-23  
**Status**: Draft  
**Input**: User description: "Monitoring, Metrics & Health Endpoint — Add observability infrastructure to the blockchain node: a lightweight `/health` HTTP endpoint, Prometheus-compatible metrics, and structured log leveling with configurable output formats."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Health Check for Node Operators (Priority: P1)

A node operator or orchestrator (e.g., systemd, Kubernetes) periodically hits a simple HTTP `/health` endpoint to verify the node is running, get basic status (chain height, peer count, uptime), and detect whether intervention is needed.

**Why this priority**: This is the foundation of observability — without a health check, operators have no programmatic way to monitor node liveness or basic health. It delivers immediate value with minimal complexity.

**Independent Test**: Can be fully tested by starting a node, making an HTTP GET to the health endpoint, and verifying the JSON response contains expected fields (status, chain_height, peer_count, uptime_seconds).

**Acceptance Scenarios**:

1. **Given** a node is running, **When** an operator sends `GET /health`, **Then** the response is a JSON object with `status`, `chain_height`, `peer_count`, `chunk_count`, `uptime_seconds`, and `last_block_index` fields
2. **Given** a node is running with no peers connected, **When** an operator sends `GET /health`, **Then** the response has `peer_count` of 0 and status is still `healthy`
3. **Given** a node is shutting down (SIGINT received), **When** an operator sends `GET /health`, **Then** the response has `status` of `"shutting_down"` with current metrics
4. **Given** a node is running, **When** an operator sends a request to a non-health/metrics path, **Then** the server returns a 404 response

---

### User Story 2 - Prometheus Metrics Scraping (Priority: P2)

An operator running Prometheus (or a compatible scraper) periodically fetches the `/metrics` endpoint to collect time-series data for dashboards and alerting on chain progress, peer connectivity, RPC throughput, and error rates.

**Why this priority**: Metrics enable proactive alerting and trend analysis. This builds on the health endpoint infrastructure and delivers significant operational value for production deployments.

**Independent Test**: Can be fully tested by starting a node, performing several RPC requests and block operations, then fetching `/metrics` and verifying that gauge and counter values reflect the actual node state.

**Acceptance Scenarios**:

1. **Given** a node is running with a chain of 50 blocks, **When** a scraper sends `GET /metrics`, **Then** the response includes `chain_height` gauge with value 50
2. **Given** a node has processed 100 RPC requests with 3 errors, **When** a scraper sends `GET /metrics`, **Then** the response includes `rpc_requests_total` counter at 100 and `rpc_errors_total` counter at 3
3. **Given** a node has received 200 blocks from peers and rejected 5 as invalid, **When** a scraper sends `GET /metrics`, **Then** the response includes `blocks_received_total` at 200 and `blocks_rejected_total` at 5
4. **Given** a node is connected to 3 peers, **When** a scraper sends `GET /metrics`, **Then** the response includes `peer_count` gauge at 3

---

### User Story 3 - Structured Logging for Log Aggregation (Priority: P3)

An operator configures the node to output logs in JSON format so that log aggregation tools (e.g., Loki, ELK) can parse, search, and correlate log entries with metrics and traces.

**Why this priority**: Structured logging is a quality-of-life improvement for operations. It's less critical than health/metrics endpoints but enables efficient troubleshooting at scale.

**Independent Test**: Can be fully tested by starting a node with JSON log format enabled, triggering various log levels (info, warn, error), and verifying that each log line is valid JSON with `level`, `timestamp`, `message`, and `component` fields.

**Acceptance Scenarios**:

1. **Given** a node is configured with `log_format: "json"`, **When** the node emits log messages, **Then** each line is valid JSON containing `level`, `timestamp`, `message`, and `component` fields
2. **Given** a node is configured with `log_level: "warn"`, **When** the node emits messages at various levels, **Then** only `warn` and `error` level messages appear in output
3. **Given** a node is configured with default settings, **When** the node emits log messages, **Then** output uses human-readable text format

---

### Edge Cases

- When the health endpoint is hit during shutdown, it returns `status: "shutting_down"` with current metrics (served normally, then server stops accepting connections)
- Metrics endpoint handles concurrent access via `std::atomic` counters and mutex-protected gauge values
- When the chain is empty (only genesis block), `chain_height` is 0 and `last_block_index` is -1
- Structured JSON logging escapes special characters (newlines, quotes, non-UTF-8 bytes) per JSON spec using nlohmann/json serialization

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide an HTTP `/health` endpoint that returns a JSON object containing `status` (string: `"healthy"` or `"shutting_down"`), `chain_height` (integer), `peer_count` (integer), `chunk_count` (integer), `uptime_seconds` (number), and `last_block_index` (integer); status is `"shutting_down"` when `IBlockchain::isShuttingDown()` returns true
- **FR-002**: System MUST provide an HTTP `/metrics` endpoint that returns Prometheus-compatible text format with gauge and counter metrics
- **FR-003**: System MUST expose the following Prometheus metrics: `chain_height` (gauge), `peer_count` (gauge), `chunk_count` (gauge), `active_connections` (gauge), `uptime_seconds` (gauge), `rpc_requests_total` (counter), `rpc_errors_total` (counter), `blocks_received_total` (counter), `blocks_rejected_total` (counter)
- **FR-004**: System MUST provide a MetricsService that collects and updates metrics from the blockchain state and peer network without blocking or invasive changes to existing components
- **FR-005**: System MUST support configurable log levels (trace, debug, info, warn, error) via `config.json`; `LogLevel` enum extended with `Trace = 0` (shifting Debug→1, Info→2, Warning→3, Error→4), new `LOG_TRACE` macro added
- **FR-006**: System MUST support JSON structured log output format configurable via `network.log_format` in `config.json` (values: `"text"` default, `"json"`); implemented by replacing `logMessage()` to detect format and output accordingly; `LOG_*` macros unchanged
- **FR-007**: System MUST allow the monitoring HTTP server port and bind address to be configured via `network.monitoring_port` and `network.monitoring_bind_address` in `config.json`, plus `--monitoring-port` CLI argument
- **FR-008**: System MUST disable the monitoring HTTP server by default (opt-in via configuration); when enabled, default bind address is `127.0.0.1` (localhost only) and default port is `9090`
- **FR-009**: System MUST handle concurrent requests to health/metrics endpoints without data corruption
- **FR-010**: System MUST gracefully stop the monitoring HTTP server during node shutdown

### Key Entities

- **MetricsService**: Central component that maintains current metric values, observes blockchain and network state changes, and formats output for the metrics endpoint
- **HealthResponse**: JSON structure returned by `/health` containing node status snapshot
- **MonitoringHttpServer**: Lightweight HTTP server hosting `/health` and `/metrics` endpoints on a separate port from the JSON-RPC server

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Health endpoint responds to requests within 50ms under normal operating conditions
- **SC-002**: Metrics values reflect actual node state with less than 5 seconds of staleness
- **SC-003**: Monitoring HTTP server adds less than 5% overhead to node CPU/memory usage when serving 10 requests/second
- **SC-004**: Structured JSON log output is parseable by standard JSON parsers with 100% success rate
- **SC-005**: Node operators can determine node health status and key metrics without accessing the JSON-RPC API

## Clarifications

### Session 2026-06-23

- Q: Metrics collection strategy — callback hooks vs polling vs hybrid? → A: Lightweight callback hooks added to existing components (PeerManager, RpcServer, Blockchain); MetricsService registers callbacks that fire on state changes
- Q: Log level `trace` vs existing 4-level enum — add Trace or drop it? → A: Add `Trace` to `LogLevel` enum as the lowest level (shift Debug=0→1, etc.), add `LOG_TRACE` macro
- Q: Monitoring server bind address — localhost only, all interfaces, or configurable? → A: Default to `127.0.0.1` (localhost), configurable via `network.monitoring_bind_address` and `network.monitoring_port` in `config.json`
- Q: Structured logging scope — replace logMessage, new logger, or parallel? → A: Replace `logMessage()` implementation to detect format config and output JSON or text; macros unchanged
- Q: Health endpoint during shutdown — return shutting_down, 503, or healthy? → A: Return `status: "shutting_down"` with current metrics; serve response then stop accepting new connections

## Assumptions

- The monitoring HTTP server runs on a separate port from the existing JSON-RPC server (e.g., default 9090)
- Existing Boost.Asio infrastructure can be reused for the HTTP server (shared io_context)
- Prometheus text format follows the standard Exposition format (one metric per line, labels in curly braces)
- Log level ordering is: trace < debug < info < warn < error (configuring "warn" shows warn and error)
- The feature reuses the existing `config.json` configuration pattern established by spec 009
- Metrics are collected via callback hooks: MetricsService registers `std::function<void()>` callbacks on PeerManager, RpcServer, and Blockchain; no polling timers used
- No authentication or TLS is required for the monitoring endpoints (operators are expected to manage access via firewall/port binding)
- The monitoring server handles only synchronous GET requests (no POST, PUT, or streaming)
