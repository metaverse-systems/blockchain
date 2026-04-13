# Feature Specification: Address Test Quality

**Feature Branch**: `020-address-test-quality`  
**Created**: 2026-04-13  
**Status**: Draft  
**Input**: User description: "Address Test Quality"

## User Scenarios & Testing

### User Story 1 - Replace Trivial Assertions with Meaningful Checks (Priority: P1)

As a developer, I want every test case to assert meaningful behavioral outcomes so that the test suite catches real regressions rather than only verifying "no crash."

**Why this priority**: Trivial assertions (`REQUIRE(true)`, `SUCCEED(...)`) provide zero regression-detection value. They give false confidence that code works correctly when in fact no behavior is being verified. This is the single highest-impact improvement to test quality.

**Independent Test**: Can be verified by reviewing each updated test and confirming that every assertion checks a specific, observable side-effect or return value of the code under test.

**Acceptance Scenarios**:

1. **Given** a test file containing `REQUIRE(true)` or `SUCCEED(...)` as its only assertion, **When** the trivial assertion is replaced with a behavioral check, **Then** the test fails if the behavior it claims to verify is broken.
2. **Given** the "Server Construction" test in `server_tests.cpp`, **When** the test runs, **Then** it asserts observable properties of the constructed server (e.g., listening state, bound port, or SSL context configuration) instead of `REQUIRE(true)`.
3. **Given** the "Periodic timer skips save when not dirty" test in `chunk_persistence_tests.cpp`, **When** the timer fires on a non-dirty chain, **Then** the test asserts that no save operation occurred (e.g., chunk file modification time unchanged or save-call count is zero).

---

### User Story 2 - Make Vacuously-Passing Tests Verify Real Behavior (Priority: P1)

As a developer, I want tests whose pass/fail outcome actually depends on the behavior under test so that a broken implementation causes the corresponding test to fail.

**Why this priority**: Vacuously-passing tests are equally dangerous as trivial assertions — they silently pass regardless of whether the feature works, hiding real bugs.

**Independent Test**: For each updated test, temporarily break the behavior it claims to verify. Confirm the test now fails.

**Acceptance Scenarios**:

1. **Given** the "Rate limiter allows up to limit then rejects" test, **When** the rate limiter is functioning, **Then** the test asserts that blocks beyond the limit are actually rejected (not just that a counter was incremented).
2. **Given** the "Pending pool capacity eviction" test, **When** the pool exceeds capacity, **Then** the test asserts the pool size equals the capacity limit and that the oldest entry was evicted.
3. **Given** the "Difficulty cache invalidated on replaceChain" test, **When** `replaceChain()` is called, **Then** the test asserts that a subsequent difficulty query recomputes rather than returning a stale cached value.
4. **Given** the "Chain reorg deeper than maxReorgDepth is rejected" test, **When** a deep reorg is attempted, **Then** the test asserts that the chain state is unchanged *and* verifies the reorg would have been accepted if the depth check were absent (e.g., candidate chain is otherwise valid and longer).

---

### User Story 3 - Rewrite RPC Expansion Tests to Exercise Real Handlers (Priority: P1)

As a developer, I want `rpc_expansion_tests.cpp` to invoke actual RPC handler logic so that tests detect regressions in RPC response generation, error codes, and parameter validation.

**Why this priority**: Every test in this file currently constructs JSON response objects manually and asserts their structure — zero actual RPC code is exercised. The entire file provides no regression protection for any RPC endpoint.

**Independent Test**: Delete or break an RPC handler method and confirm that at least one test in this file fails.

**Acceptance Scenarios**:

1. **Given** the `rpc_expansion_tests.cpp` test file, **When** tests run, **Then** each test invokes the actual extracted RPC handler method directly with a mocked `IBlockchain`.
2. **Given** an RPC handler that returns an error for invalid input, **When** the test sends invalid input, **Then** the test asserts the correct JSON-RPC error code and error message are returned by the real handler.
3. **Given** a valid RPC request for a query endpoint (e.g., `getBlockByIndex`, `getChainLength`), **When** the test sends the request, **Then** the response contains correct data reflecting the actual blockchain state.

---

### User Story 4 - Make Integration Tests Deterministic (Priority: P2)

As a developer, I want integration tests to produce consistent results regardless of system load so that CI pipelines do not suffer flaky test failures.

**Why this priority**: Timing-dependent tests cause intermittent CI failures that erode developer confidence and waste time investigating false positives. This is important but secondary to fixing tests that verify nothing at all.

**Independent Test**: Run the updated integration tests under artificial load (e.g., CPU stress) and confirm they pass reliably.

**Acceptance Scenarios**:

1. **Given** the `p2p_sync_integration_tests.cpp` suite, **When** tests run on a machine under heavy load, **Then** tests pass reliably because they use deterministic event-loop advancement or condition-based waiting instead of fixed-duration sleeps.
2. **Given** the `rpc_integration_tests.cpp` suite, **When** tests run, **Then** connection readiness is verified before issuing RPC calls rather than assuming the server is ready after a fixed delay.
3. **Given** `chunk_persistence_tests.cpp` timer tests, **When** the test triggers a periodic timer, **Then** it uses deterministic event-loop advancement instead of fixed-duration waits.

---

### User Story 5 - Close High-Priority Coverage Gaps (Priority: P2)

As a developer, I want test coverage for critical untested behaviors so that failures in those code paths are detected before reaching production.

**Why this priority**: Several high- and medium-severity behaviors identified in the audit have no test coverage. Covering them reduces the risk of undetected regressions in important code paths.

**Independent Test**: Each new test can be run standalone and verifies a specific previously-untested behavior.

**Acceptance Scenarios**:

1. **Given** the `saveAllChunks()` method where one chunk fails to save, **When** the failure occurs, **Then** a test verifies the remaining chunks are still saved and the caller is notified of the partial failure.
2. **Given** a peer that disconnects mid-propagation, **When** the disconnect occurs during block relay, **Then** a test verifies the system handles the disconnection gracefully without crashing or corrupting state.
3. **Given** the rate limiter after its time window expires, **When** a new block arrives after the window reset, **Then** a test verifies the block is accepted (the counter was reset).
4. **Given** the pending pool with blocks that have exceeded their TTL, **When** the expiry check runs, **Then** a test verifies stale blocks are removed from the pool.
5. **Given** a block propagation relay, **When** a block is relayed to peers, **Then** a test verifies the original sender is excluded from the relay set.
6. **Given** `recoverChain()` with corrupted index files, **When** recovery runs, **Then** a test verifies the system falls back to rebuilding from chunk files.

---

### Edge Cases

- What happens when a test's setup or teardown fails mid-way — does the test harness report the failure clearly rather than hanging?
- How do tests behave when the blockchain data directory is read-only or missing — do they produce clear error diagnostics?
- What happens when integration tests run concurrently — do they use isolated data directories to avoid interference?

## Requirements

### Functional Requirements

- **FR-001**: Every test case across all 26 test files in the suite MUST contain at least one assertion that checks an observable side-effect or return value of the code under test. No test may use `REQUIRE(true)` or `SUCCEED(...)` as its sole assertion. This requires a comprehensive audit of all ~150+ test cases, not only the ~19 flagged in the audit.
- **FR-002**: Every test case MUST fail when the specific behavior it claims to verify is deliberately broken.
- **FR-003**: RPC endpoint tests MUST invoke actual handler logic by calling extracted handler methods directly with a mocked `IBlockchain` dependency. Tests MUST NOT construct expected response objects manually and assert their structure without exercising production code. Socket-based integration coverage is already provided by `rpc_integration_tests.cpp` and need not be duplicated.
- **FR-004**: Integration tests MUST NOT depend on wall-clock sleep durations for correctness. Waiting logic MUST use deterministic event-loop advancement, condition-based polling with bounded retries, or signaling mechanisms.
- **FR-005**: Test coverage MUST be added for each open high-severity coverage gap identified in the audit: partial `saveAllChunks()` failure. Production code error-reporting contracts (e.g., return values, exceptions) MAY be modified where necessary to make the failure observable to callers and testable.
- **FR-006**: Test coverage MUST be added for each open medium-severity coverage gap identified in the audit: peer disconnect during propagation, rate limiter window reset, pending pool TTL expiry, block relay sender exclusion, and `recoverChain()` with corrupted indexes.
- **FR-007**: Each integration test MUST use an isolated temporary data directory that is created during setup and cleaned up during teardown.
- **FR-008**: The `IBlockchain` interface MUST be narrowed into read and write sub-interfaces (e.g., `IChainReader` and `IChainWriter`) so that consumers like `RpcServer` that only need query access are not coupled to the full mutation surface. This improves testability by allowing lightweight mocks scoped to the actual dependency.

## Success Criteria

### Measurable Outcomes

- **SC-001**: Zero test cases in the suite use `REQUIRE(true)` or `SUCCEED(...)` as their only assertion.
- **SC-002**: For every test claiming to verify a specific behavior, deliberately breaking that behavior causes the test to fail (mutation-testing principle).
- **SC-003**: All RPC endpoint tests exercise actual production handler code — no manually-constructed response objects are asserted without invoking real logic.
- **SC-004**: Integration tests pass reliably across 10 consecutive local test runs with no flaky failures attributable to timing. CI-level verification is post-merge best-effort.
- **SC-005**: All open high- and medium-severity test coverage gaps from the audit (6 items) have corresponding test cases.
- **SC-006**: The full test suite completes within a reasonable time bound, with no individual test requiring more than 30 seconds.

## Clarifications

### Session 2026-04-13

- Q: Does FR-001 apply to all ~150+ tests or only the ~19 flagged in the audit? → A: All ~150+ test cases across the entire suite must be audited and fixed.
- Q: Should rewritten RPC expansion tests call handlers directly or go through a socket to a running server? → A: Call extracted handler methods directly (unit-test style), mock only IBlockchain.
- Q: Should production code error-reporting contracts be modified where needed for coverage-gap testability? → A: Yes, production code changes to error-reporting contracts are in scope where necessary.
- Q: What is explicitly out of scope — test framework migration, coverage tooling, architecture refactoring (§6.1–6.3)? → A: Architecture refactoring for testability (§6.1 narrow IBlockchain interface) is in scope; test framework migration and code coverage tooling/thresholds are out of scope.
- Q: How should SC-004 (10 consecutive flake-free runs) be validated during implementation? → A: Validate with local repeated runs; CI verification is post-merge best-effort.

## Assumptions

- The existing Catch2 test framework will continue to be used; no test framework migration is in scope.
- Code coverage tooling integration (e.g., gcov, lcov) and coverage threshold enforcement are out of scope.
- Architecture concerns §6.2 (service layer between network and domain) and §6.3 (error handling consistency) are out of scope for this feature.
- Production code will be modified where necessary to improve testability, including both structural changes (e.g., narrowing `IBlockchain` into reader/writer interfaces, extracting RPC handler functions) and error-reporting contract changes (e.g., making `saveAllChunks()` report partial failures to callers). Behavioral changes unrelated to testability are out of scope.
- The shared `TestHelpers` namespace established in the prior feature (019) will be extended as needed for new helper functions.
- Flaky test elimination targets the known timing-dependent tests identified in the audit; discovering additional flaky tests is best-effort.
- The existing `rpc_integration_tests.cpp` approach (socket-based testing against a running server) serves as the reference pattern for rewriting `rpc_expansion_tests.cpp`.
