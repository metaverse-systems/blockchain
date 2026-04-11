# Feature Specification: Integration Test Suite

**Feature Branch**: `012-integration-test-suite`  
**Created**: 2026-04-11  
**Status**: Draft  
**Input**: User description: "Implement 012 — Integration Test Suite per roadmap"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Verify RPC Endpoints Over Real TLS (Priority: P1)

A developer runs the integration test suite to confirm that all JSON-RPC methods work end-to-end over actual TLS connections, producing correct responses and proper error handling — without relying on mocked I/O.

**Why this priority**: RPC is the primary external interface for clients. Validating it over real TLS connections catches protocol errors, serialization bugs, and certificate issues that unit tests with mocked transports will never reveal.

**Independent Test**: Can be fully tested by starting a single blockchain node with a self-signed certificate, connecting a TLS client, and issuing every registered RPC method. Delivers confidence that the RPC surface works as documented.

**Acceptance Scenarios**:

1. **Given** a freshly started blockchain node with self-signed TLS certificates, **When** a test client sends a valid `addBlock` JSON-RPC request over TLS, **Then** the node returns a success response and the chain length increases by one.
2. **Given** a running node, **When** a test client sends each documented RPC method (`getChainLength`, `getChunkCount`, `getNodeStatus`, `getBlockRange`, `getBlockHeader`, `getInclusionProof`, `verifyInclusionProof`, `requestSync`, `publish`, `getStreamEntries`), **Then** each method returns a well-formed JSON-RPC response matching the expected schema.
3. **Given** a running node, **When** a test client sends a malformed JSON-RPC request, **Then** the node returns a JSON-RPC error response with an appropriate error code — it does not crash or hang.
4. **Given** a running node, **When** a test client sends a request for an unknown method, **Then** the node returns a "method not found" JSON-RPC error.

---

### User Story 2 - Two-Node P2P Sync Over Real TLS (Priority: P2)

A developer runs the integration tests to confirm that two blockchain nodes can connect over TLS, perform peer discovery, synchronize their chains, and propagate new blocks — exercising the real P2P protocol stack.

**Why this priority**: Multi-node behavior is the core value proposition of a blockchain. Verifying sync, propagation, and peer exchange over real connections is essential for production readiness, but depends on the single-node RPC infrastructure from US1.

**Independent Test**: Can be tested by starting two nodes with separate data directories and self-signed certificates, configuring one as a seed peer of the other, then verifying chain synchronization completes and new blocks propagate to both nodes.

**Acceptance Scenarios**:

1. **Given** two newly started nodes where Node A has 10 blocks and Node B has only the genesis block, **When** Node B connects to Node A as a peer, **Then** Node B's chain length eventually matches Node A's chain length.
2. **Given** two synchronized nodes, **When** Node A mines a new block, **Then** Node B receives the block via propagation and its chain length increases by one.
3. **Given** two connected nodes, **When** a third node starts and connects to one of them, **Then** the third node discovers the other peer via peer exchange and can synchronize from either.

---

### User Story 3 - Automated Test Execution Via Build System (Priority: P3)

A developer or CI system runs a single build-system command to compile and execute all integration tests, receiving a clear pass/fail report — enabling adoption in continuous integration pipelines.

**Why this priority**: Without build-system integration the tests exist but cannot be run reliably in CI. This is the enablement layer for spec 013 (CI/CD Pipeline) and daily developer workflows.

**Independent Test**: Can be tested by running the integration test build target and verifying that it compiles, executes, and produces a Catch2 test report with zero failures on a clean build.

**Acceptance Scenarios**:

1. **Given** a fresh checkout with all dependencies installed, **When** the developer runs the build command targeting integration tests, **Then** all integration test binaries compile without errors.
2. **Given** compiled integration test binaries, **When** the developer executes each binary, **Then** every test case passes and the exit code is zero.
3. **Given** a failing integration test, **When** the binary is executed, **Then** the output clearly identifies which test case failed and with what assertion.

---

### Edge Cases

- What happens when the TLS certificate is expired or self-signed with a different CA? Tests must use their own generated test certificates, not rely on system trust stores.
- What happens when a port is already in use? Tests must select available ports dynamically to avoid conflicts with parallel test runs or running daemons.
- What happens when a node takes too long to start? Tests must have timeouts so they fail fast rather than hanging indefinitely.
- What happens when two-node tests leave zombie processes? Test fixtures must ensure all spawned nodes are stopped during teardown, even on assertion failure.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide a Catch2 integration test binary that exercises RPC endpoints over real TLS connections using self-signed test certificates.
- **FR-002**: System MUST provide a Catch2 integration test binary that exercises P2P sync and block propagation between two blockchain nodes communicating over real TLS connections.
- **FR-003**: All integration tests MUST generate their own TLS test certificates at test setup time (self-signed), not depend on pre-existing or system-installed certificates.
- **FR-004**: All integration tests MUST use dynamically assigned ports (e.g., binding to port 0 and reading back the assigned port) to avoid conflicts.
- **FR-005**: All integration test fixtures MUST enforce timeouts on node startup, RPC responses, and sync completion to prevent hanging tests.
- **FR-006**: All integration test fixtures MUST guarantee node process cleanup during teardown, including on test failure or unexpected exceptions.
- **FR-007**: Integration test binaries MUST be registered as build targets in the existing Makefile-based build system so they compile with the standard build command.
- **FR-008**: RPC integration tests MUST cover all documented JSON-RPC methods: `addBlock`, `getChainLength`, `getChunkCount`, `getNodeStatus`, `getBlockRange`, `getBlockHeader`, `getInclusionProof`, `verifyInclusionProof`, `requestSync`, `publish`, `getStreamEntries`.
- **FR-009**: RPC integration tests MUST validate error responses for malformed requests and unknown methods.
- **FR-010**: P2P integration tests MUST verify full-chain sync from a node that is ahead to a node that is behind.
- **FR-011**: P2P integration tests MUST verify block propagation from the mining node to a connected peer.

### Key Entities

- **Test Fixture**: Manages the lifecycle of one or more blockchain node instances (start, wait-for-ready, stop, cleanup) within a test case.
- **TLS Test Certificate**: Self-signed X.509 certificate and private key generated per test run for node-to-node and client-to-node TLS.
- **RPC Test Client**: A TLS-capable client that sends JSON-RPC requests to a running node and parses responses.
- **Node Instance**: A blockchain node running in-process on a separate thread with its own data directory, RPC port, and P2P port.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All integration tests pass on a clean build from a fresh checkout with no manual setup beyond installing system dependencies.
- **SC-002**: The integration test suite exercises every documented JSON-RPC endpoint with at least one positive and one negative test case each.
- **SC-003**: The two-node P2P test confirms chain synchronization completes within 30 seconds for a 10-block chain.
- **SC-004**: No integration test leaves behind zombie node processes or temporary files after completion, even when a test fails.
- **SC-005**: Integration tests can run in parallel with other test binaries without port conflicts or filesystem collisions.
- **SC-006**: Each integration test completes within 60 seconds under normal conditions.

## Clarifications

### Session 2026-04-11

- Q: Should integration tests run the blockchain node in-process (on separate threads) or as child processes (spawning the daemon)? → A: In-process on separate threads within the test binary.

## Assumptions

- The existing Catch2 test framework (already used for unit tests) will be reused for integration tests.
- Self-signed TLS certificates generated at test time are sufficient; no need for a real CA or certificate authority chain.
- Tests run on a single machine — multi-machine distributed testing is out of scope.
- The existing Makefile.am / autotools build system is the target; no migration to CMake or other build systems.
- Dynamic port allocation (binding to port 0) is supported by the OS on all target platforms.
- Integration tests run blockchain node instances in-process on separate threads within the test binary, enabling deterministic lifecycle control, simpler debugging, and RAII-based cleanup.
- Test data directories use temporary filesystem paths and are cleaned up after each test.
