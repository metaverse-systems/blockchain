# Implementation Plan: Monitoring, Metrics & Health Endpoint

**Branch**: `023-monitoring-metrics-health` | **Date**: 2026-06-23 | **Spec**: [spec.md](./spec.md)
**Input**: Feature specification from `/specs/023-monitoring-metrics-health/spec.md`

## Summary

Add observability infrastructure to the blockchain node: a lightweight HTTPS `/health` endpoint, Prometheus-compatible `/metrics` endpoint, and structured JSON log output. The monitoring HTTPS server runs on a separate port (default 9090, localhost-only default) and is opt-in via `config.json`. Bind address is configurable (IPv4/IPv6). Metrics are collected via lightweight callback hooks registered with existing components (PeerManager, RpcServer, Blockchain) — no polling timers. Log format switching (text → JSON) is implemented by replacing the `logMessage()` implementation.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)  
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`), Catch2 (test only)  
**Storage**: Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`), JSON files (`peers.json`, `config.json`) — no new persistence  
**Testing**: Catch2 (test framework), existing mock patterns (`MockChunk`, `MockSessionHandler`, `MockAcceptor`)  
**Target Platform**: Linux, macOS, Windows (cross-platform)  
**Project Type**: CLI application with network server (blockchain node)  
**Performance Goals**: Health endpoint <50ms response, <5% CPU/memory overhead at 10 req/s, metrics staleness <5s  
**Constraints**: Monitoring server optional (disabled by default), bind address configurable (default 127.0.0.1), IPv4/IPv6 support, TLS required on monitoring port, reuse existing Boost.Asio io_context, no polling timers  
**Scale/Scope**: Single node monitoring, operator-facing (not P2P), synchronous GET-only endpoints

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. C++20 | ✅ PASS | All new code will use C++20 |
| II. Autotools + `-j8` | ✅ PASS | New sources added to `src/Makefile.am`, tests to `tests/Makefile.am` |
| III. Full Test Coverage | ✅ PASS | Unit tests for MetricsService, MonitoringHttpServer, JSON logging; integration tests for HTTP endpoints |
| IV. Code Style | ✅ PASS | Will follow existing conventions (pragma once, naming, indentation) |
| V. Minimal Dependencies | ✅ PASS | No new dependencies — reuse Boost.Asio, nlohmann/json, Catch2 |
| VI. Mandatory TLS | ✅ PASS | Monitoring server uses TLS (ssl::stream), consistent with constitution requirement |
| VII. Cross-Platform | ✅ PASS | IPv4/IPv6 dual-stack via Boost.Asio, cross-platform on Linux/macOS/Windows |
| VIII. Feature Branches | ✅ PASS | Working on `023-monitoring-metrics-health` branch |
| IX. Pre-1.0 API Stability | ✅ PASS | New endpoints, no backward compat concerns |
| X. Low-Latency | ✅ PASS | Lightweight callback hooks, no polling, atomic counters |
| XI. MIT License | ✅ PASS | All new files will include MIT-compatible headers |
| XII. .gitignore | ✅ PASS | Will add entries for any new artifacts |
| XIII. ROADMAP.md | ✅ PASS | Will update after feature completion |

**TLS Implementation**: The monitoring HTTPS server uses TLS via `boost::asio::ssl::stream<tcp::socket>`, consistent with the constitution's Mandatory TLS principle (§VI). It reuses the same TLS infrastructure as RpcServer and PeerServer (shared `ssl::context`). Self-signed certificate is acceptable for localhost use; operators can replace with CA-signed certs for production deployments.

---

### Post-Design Constitution Re-Check (after Phase 1)

| Principle | Status | Notes |
|-----------|--------|-------|
| I. C++20 | ✅ PASS | All new code uses C++20 |
| II. Autotools + `-j8` | ✅ PASS | New sources in `src/Makefile.am`, tests in `tests/Makefile.am` |
| III. Full Test Coverage | ✅ PASS | Unit + integration tests planned for all new components |
| IV. Code Style | ✅ PASS | Following existing `#pragma once`, naming, indentation conventions |
| V. Minimal Dependencies | ✅ PASS | No new dependencies — reuse Boost.Asio, nlohmann/json, Catch2 |
| VI. Mandatory TLS | ✅ PASS | Monitoring server uses TLS (ssl::stream), consistent with all network interfaces |
| VII. Cross-Platform | ✅ PASS | IPv4/IPv6 dual-stack via Boost.Asio, cross-platform on Linux/macOS/Windows |
| VIII. Feature Branches | ✅ PASS | `023-monitoring-metrics-health` branch |
| IX. Pre-1.0 API Stability | ✅ PASS | New endpoints only, no backward compat concerns |
| X. Low-Latency | ✅ PASS | Atomic counters, no polling, no blocking I/O on hot paths |
| XI. MIT License | ✅ PASS | All new files will include MIT header |
| XII. .gitignore | ✅ PASS | Will update with new test binary entries |
| XIII. ROADMAP.md | ✅ PASS | Will update after feature completion |

**All gates pass**. No exemptions required.

## Project Structure

### Documentation (this feature)

```text
specs/[###-feature]/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
src/
├── MonitoringHttpServer.hpp   (new — HTTPS server for /health and /metrics, TLS via ssl::stream)
├── MonitoringHttpServer.cpp   (new — TLS, IPv4/IPv6 configurable bind, GET-only)
├── MetricsCollector.hpp       (new — atomic counters + gauge computation)
├── MetricsCollector.cpp       (new — Prometheus text format output)
├── utils.hpp                  (modify — LogLevel::Trace, LOG_TRACE, logMessage JSON)
├── utils.cpp                  (modify — logMessage JSON output, parseLogLevel "trace")
├── NodeConfig.hpp             (modify — monitoring fields in NetworkConfig)
├── NodeConfig.cpp             (modify — parse/generate monitoring config)
├── CliParser.hpp              (modify — monitoring_port in CliOptions)
├── CliParser.cpp              (modify — parse --monitoring-port)
├── main.cpp                   (modify — create/start MonitoringHttpServer)
├── PeerManager.hpp            (modify — MetricsCollector* member)
├── PeerManager.cpp            (modify — metric hooks on peer events)
├── network/RpcServer.hpp      (modify — MetricsCollector* member)
├── network/RpcServer.cpp      (modify — metric hooks on RPC events)
├── BlockPropagation.hpp       (modify — MetricsCollector* member)
└── BlockPropagation.cpp       (modify — metric hooks on block events)

tests/
├── monitoring_tests.cpp                (new — unit tests for MetricsCollector, MonitoringHttpServer)
├── monitoring_integration_tests.cpp    (new — HTTP endpoint integration tests)
└── Makefile.am                         (modify — add new test binaries)

Makefile.am files to modify:
├── src/Makefile.am                     (modify — add new source files)
└── tests/Makefile.am                   (modify — add new test targets)
```

**Structure Decision**: Single project layout. All new monitoring code lives in `src/` alongside existing components. No new subdirectories needed — MonitoringHttpServer and MetricsCollector are self-contained classes. Test files follow existing naming convention (`*_tests.cpp`). The monitoring server is a separate HTTPS listener (TLS via ssl::stream) on a configurable port with IPv4/IPv6 support, distinct from the JSON-RPC server.

## Complexity Tracking

> **No violations — all principles pass.**
