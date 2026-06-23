# Tasks: Monitoring, Metrics & Health Endpoint

**Input**: Design documents from `/specs/023-monitoring-metrics-health/`
**Prerequisites**: plan.md, spec.md, data-model.md, research.md, contracts/
**Tests**: Included (unit + integration tests for monitoring components)

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization — add new source files to build system

- [ ] T001 Add new source files to `src/Makefile.am` (MonitoringHttpServer.cpp, MetricsCollector.cpp) and rebuild with `make -j8`
- [ ] T002 [P] Add new test targets to `tests/Makefile.am` (monitoring_tests, monitoring_integration_tests) and rebuild with `make -j8`
- [ ] T002b [P] Update `.gitignore` to exclude new test binaries (`monitoring_tests`, `monitoring_integration_tests`)

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure that MUST be complete before ANY user story can be implemented

**⚠️ CRITICAL**: No user story work can begin until this phase is complete

- [ ] T003 Implement MetricsCollector class in `src/MetricsCollector.hpp` (atomic counters: rpc_requests_total_, rpc_errors_total_, blocks_received_total_, blocks_rejected_total_; start_time_; increment methods; Prometheus text format output method)
- [ ] T004 Implement MetricsCollector in `src/MetricsCollector.cpp` (Prometheus exposition format v0.0.4 output with HELP/TYPE comments, gauge computation of chain_height/peer_count/chunk_count/active_connections/uptime_seconds from component state)
- [ ] T005 [P] Add monitoring config fields to `src/NodeConfig.hpp` (monitoring_enabled, monitoring_port, monitoring_bind_address, log_format in NetworkConfig struct)
- [ ] T006 [P] Extend LogLevel enum in `src/utils.hpp` (add Trace = 0, shift Debug=1, Info=2, Warning=3, Error=4; add LOG_TRACE macro)
- [ ] T007 Modify `src/NodeConfig.cpp` to parse/generate monitoring config fields from config.json
- [ ] T008 Modify `src/CliParser.hpp` and `src/CliParser.cpp` to add --monitoring-port CLI argument
- [ ] T009 [P] Implement MonitoringHttpServer class in `src/MonitoringHttpServer.hpp` (TLS HTTPS server via ssl::stream, IPv4/IPv6 support, acceptor, GET-only handler)
- [ ] T010 Implement MonitoringHttpServer in `src/MonitoringHttpServer.cpp` (TLS handshake, HTTP request parsing with async_read_until, /health and /metrics route dispatch, 404 for unknown paths, graceful shutdown)
- [ ] T011 Modify `src/main.cpp` to create and start MonitoringHttpServer when monitoring_enabled is true (reuse existing ssl::context from RpcServer)

**Checkpoint**: Foundation ready — MetricsCollector, MonitoringHttpServer, config parsing, and build system are all in place. User story implementation can now begin.

---

## Phase 3: User Story 1 - Health Check for Node Operators (Priority: P1) 🎯 MVP

**Goal**: Node operators can hit `/health` endpoint to verify node liveness and get basic status (chain height, peer count, uptime, chunk count, last block index).

**Independent Test**: Start a node with monitoring enabled, `curl -k https://localhost:9090/health`, verify JSON response contains all expected fields with correct values.

### Tests for User Story 1 ⚠️

- [ ] T012 [P] [US1] Write unit tests for HealthResponse JSON construction in `tests/monitoring_tests.cpp` (verify all fields present, correct types, edge cases: empty chain chain_height=0/last_block_index=-1, no peers peer_count=0)

### Implementation for User Story 1

- [ ] T013 [US1] Implement /health endpoint handler in `src/MonitoringHttpServer.cpp` (build HealthResponse JSON with status, chain_height, peer_count, chunk_count, uptime_seconds, last_block_index; check isShuttingDown() for status; return application/json 200 OK)
- [ ] T014 [US1] Implement /health 404 handler in `src/MonitoringHttpServer.cpp` (return "Not Found" with text/plain for unknown paths)
- [ ] T015 [US1] Write integration test for /health endpoint in `tests/monitoring_integration_tests.cpp` (start MonitoringHttpServer with TLS, send HTTP GET, verify JSON response fields)

**Checkpoint**: User Story 1 is fully functional — `/health` endpoint returns correct JSON with node status, chain height, peer count, chunk count, uptime, and last block index.

---

## Phase 4: User Story 2 - Prometheus Metrics Scraping (Priority: P2)

**Goal**: Operators running Prometheus can scrape `/metrics` endpoint for time-series data on chain progress, peer connectivity, RPC throughput, and error rates.

**Independent Test**: Start a node, perform several RPC requests and block operations, then `curl -k https://localhost:9090/metrics` and verify gauge/counter values reflect actual node state.

### Tests for User Story 2 ⚠️

- [ ] T016 [P] [US2] Write unit tests for MetricsCollector Prometheus output in `tests/monitoring_tests.cpp` (verify metric format compliance: HELP/TYPE comments, gauge/counter types, correct names with blockchain_ prefix, proper values)

### Implementation for User Story 2

- [ ] T017 [US2] Implement /metrics endpoint handler in `src/MonitoringHttpServer.cpp` (call MetricsCollector::generatePrometheusText(), return text/plain; version=0.0.4 with 200 OK)
- [ ] T018 [US2] Add MetricsCollector* member to `src/PeerManager.hpp` and inject metric hooks in `src/PeerManager.cpp` (peer connect/disconnect events)
- [ ] T019 [US2] Add MetricsCollector* member to `src/network/RpcServer.hpp` and inject metric hooks in `src/network/RpcServer.cpp` (RPC request received, RPC error returned)
- [ ] T020 [US2] Add MetricsCollector* member to `src/BlockPropagation.hpp` and inject metric hooks in `src/BlockPropagation.cpp` (block received, block rejected)
- [ ] T021 [US2] Wire MetricsCollector instance creation in `src/main.cpp` and pass pointer to PeerManager, RpcServer, Blockchain, BlockPropagation constructors
- [ ] T022 [US2] Write integration test for /metrics endpoint in `tests/monitoring_integration_tests.cpp` (start server, trigger metric increments, scrape /metrics, verify counter/gauge values)

**Checkpoint**: User Stories 1 AND 2 are both functional — `/health` and `/metrics` endpoints work independently, metrics reflect actual node state.

---

## Phase 5: User Story 3 - Structured Logging for Log Aggregation (Priority: P3)

**Goal**: Operators can configure JSON structured log output so log aggregation tools can parse, search, and correlate log entries.

**Independent Test**: Start a node with `log_format: "json"`, trigger various log levels, verify each log line is valid JSON with level, timestamp, message fields.

### Tests for User Story 3 ⚠️

- [ ] T023 [P] [US3] Write unit tests for JSON log format in `tests/monitoring_tests.cpp` (verify JSON output has timestamp/level/message fields, valid JSON parsing, special character escaping, log level filtering with Trace level)

### Implementation for User Story 3

- [ ] T024 [US3] Modify `src/utils.cpp` to implement JSON structured logging in `logMessage()` (detect log_format config, output JSON Lines format with ISO 8601 UTC timestamp, uppercase level, component name, JSON-escaped message)
- [ ] T025 [US3] Modify `src/utils.cpp` to add `parseLogLevel` support for "trace" string value
- [ ] T026 [US3] Write integration test for structured logging in `tests/monitoring_integration_tests.cpp` (start node with JSON log format, verify log output is parseable JSON with expected fields)

**Checkpoint**: All user stories are independently functional — health endpoint, metrics endpoint, and structured logging all work.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Improvements that affect multiple user stories

- [ ] T027 [P] Update `docs/rpc-api.md` or create `docs/monitoring.md` with monitoring endpoint documentation (health, metrics, logging config)
- [ ] T028 Update `docs/ROADMAP.md` to mark 023-monitoring-metrics-health as complete
- [ ] T029 Verify all new files include MIT license headers
- [ ] T030 Run full test suite with `make -j8` then `./tests/monitoring_tests` and `./tests/monitoring_integration_tests` to validate end-to-end

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Setup completion — BLOCKS all user stories
- **User Stories (Phase 3+)**: All depend on Foundational phase completion
  - User stories can then proceed in parallel (if staffed)
  - Or sequentially in priority order (P1 → P2 → P3)
- **Polish (Final Phase)**: Depends on all desired user stories being complete

### User Story Dependencies

- **User Story 1 (P1)**: Can start after Foundational (Phase 2) — No dependencies on other stories
- **User Story 2 (P2)**: Can start after Foundational (Phase 2) — Depends on MetricsCollector from Phase 2, integrates with existing components
- **User Story 3 (P3)**: Can start after Foundational (Phase 2) — Depends on utils.hpp/cpp changes from Phase 2

### Within Each User Story

- Tests MUST be written and FAIL before implementation
- Core implementation before integration with existing components
- Story complete before moving to next priority

### Parallel Opportunities

- T001 and T002 (Phase 1): Different Makefile.am files, can run in parallel
- T005 and T006 (Phase 2): Different files (NodeConfig.hpp vs utils.hpp), can run in parallel
- T009 and T010 dependencies: Header first, then implementation (sequential)
- T018, T019, T020 (Phase 4): Different component files (PeerManager, RpcServer, BlockPropagation), can run in parallel
- All user stories can be worked on in parallel by different team members once Foundational is done

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup (add to build system)
2. Complete Phase 2: Foundational (MetricsCollector, MonitoringHttpServer, config)
3. Complete Phase 3: User Story 1 (/health endpoint)
4. **STOP and VALIDATE**: Test /health endpoint independently with `curl -k https://localhost:9090/health`
5. Deploy/demo if ready

### Incremental Delivery

1. Complete Setup + Foundational → Foundation ready
2. Add User Story 1 → Test /health independently → MVP!
3. Add User Story 2 → Test /metrics independently → Full metrics
4. Add User Story 3 → Test JSON logging independently → Full observability
5. Each story adds value without breaking previous stories

### Parallel Team Strategy

With multiple developers:

1. Team completes Setup + Foundational together
2. Once Foundational is done:
   - Developer A: User Story 1 (/health endpoint)
   - Developer B: User Story 2 (/metrics + metric hooks)
   - Developer C: User Story 3 (JSON structured logging)
3. Stories complete and integrate independently

---

## Notes

- [P] tasks = different files, no dependencies
- [Story] label maps task to specific user story for traceability
- Each user story should be independently completable and testable
- Verify tests fail before implementing
- Commit after each task or logical group
- Stop at any checkpoint to validate story independently
- Monitoring server uses TLS (ssl::stream) — reuse existing cert/key infrastructure
- Build with `make -j8`, run tests individually (e.g., `./tests/monitoring_tests`)
