# Feature Specification: Performance & Deduplication Cleanup

**Feature Branch**: `019-perf-dedup-cleanup`  
**Created**: 2026-04-13  
**Status**: Draft  
**Input**: User description: "Address Performance Issues and Code Duplication"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Faster Peer Operations Under Load (Priority: P1)

As a node operator running a blockchain node with many connected peers, I need peer lookups, additions, removals, and ban checks to remain fast regardless of how many peers are stored, so that the node stays responsive during high-traffic periods such as block propagation storms or peer exchange floods.

**Why this priority**: Every inbound connection attempt, peer exchange message, and block reception triggers peer lookups. With the current linear scan over up to 256 peers, these operations compound during high-activity windows and directly degrade node responsiveness. This is the highest-traffic hot path in the system.

**Independent Test**: Can be tested by measuring peer lookup/add/remove/ban-check times with a full peer list (256 entries) before and after the change. Delivers immediate latency improvement for all peer-related operations.

**Acceptance Scenarios**:

1. **Given** a node with 256 stored peers, **When** a peer lookup is performed, **Then** the lookup completes in constant time regardless of total peer count.
2. **Given** a node with 256 stored peers and a ban list of 50 entries, **When** `is_banned()` is called, **Then** the check completes in constant time regardless of ban list size.
3. **Given** a full peer list, **When** a new peer is added or an existing peer is removed, **Then** the operation completes in constant time.
4. **Given** the peer data structure has been changed, **When** `get_non_banned_peer_addresses()` is called, **Then** it still returns the correct filtered list of non-banned peer addresses.

---

### User Story 2 - Efficient RPC Request Routing (Priority: P2)

As a developer or application integrating with the node via JSON-RPC, I need RPC requests to be dispatched efficiently and the handler code to be organized into individually testable units, so that adding new RPC methods does not require modifying a single monolithic function.

**Why this priority**: The RPC interface is the primary integration surface. The current 20-method if/else chain in a 700-line callback makes it hard to maintain, test, and extend. Extracting a dispatch table improves both runtime efficiency and developer maintainability.

**Independent Test**: Can be tested by sending JSON-RPC requests for each method and verifying correct responses. Handler functions can be unit-tested in isolation.

**Acceptance Scenarios**:

1. **Given** a running node with RPC enabled, **When** any supported JSON-RPC method is called, **Then** the response is identical to the current behavior.
2. **Given** the RPC dispatch mechanism, **When** a request arrives for a supported method, **Then** the correct handler is invoked without walking a chain of string comparisons.
3. **Given** the refactored RPC handlers, **When** a new RPC method needs to be added, **Then** a developer can register it by adding a single entry to a dispatch table and implementing one handler function.
4. **Given** any unsupported method name, **When** a JSON-RPC request arrives, **Then** a standard JSON-RPC "method not found" error is returned.

---

### User Story 3 - Consolidated Packet Serialization (Priority: P2)

As a developer maintaining the P2P networking code, I need the shared packet serialization logic between PeerClient and PeerServer to live in one place, so that bug fixes or protocol changes only need to be made once.

**Why this priority**: The duplicated serialize → PacketHeader → memcpy → async_write pattern in PeerClient and PeerServer is a maintenance risk — a fix applied to one side but not the other could introduce subtle protocol bugs. Consolidating this reduces defect surface area.

**Independent Test**: Can be tested by running the existing P2P integration tests (peer connection, sync, block propagation) and verifying identical behavior after consolidation.

**Acceptance Scenarios**:

1. **Given** PeerClient sends a packet, **When** the shared serialization utility is used, **Then** the wire format is identical to the current format.
2. **Given** PeerServer sends a packet, **When** the shared serialization utility is used, **Then** the wire format is identical to the current format.
3. **Given** a protocol change to the serialization format, **When** a developer updates the shared utility, **Then** both PeerClient and PeerServer reflect the change.

---

### User Story 4 - Consolidated Test Helpers (Priority: P3)

As a developer writing or maintaining tests, I need all test utility functions (mining helpers, chain builders, temporary directory management) to live in the shared `TestHelpers.hpp` rather than being duplicated across four test files, so that test setup is consistent and easy to maintain.

**Why this priority**: While lower impact than runtime improvements, duplicated test helpers create drift risk — changes to block creation logic may not be reflected in all local copies, causing false passes or confusing failures.

**Independent Test**: Can be tested by running the full test suite and verifying all tests pass after replacing local helpers with shared ones.

**Acceptance Scenarios**:

1. **Given** the shared `TestHelpers.hpp`, **When** `sync_tests.cpp`, `block_propagation_tests.cpp`, `consensus_tests.cpp`, and `chunk_persistence_tests.cpp` are compiled, **Then** none of them define local versions of `mineTestBlock()`, `buildValidChain()`, or temporary directory helpers.
2. **Given** a change to the shared `mineTestBlock()` function, **When** all test files are recompiled, **Then** every test that uses block mining picks up the updated logic.
3. **Given** the refactored test files, **When** the full test suite is run, **Then** all tests produce the same pass/fail results as before the consolidation.

---

### User Story 5 - Reduced Unnecessary Log Allocations (Priority: P3)

As a node operator running a production node, I need log calls to avoid constructing expensive string arguments when the current log level would suppress the message, so that logging overhead is minimized in production.

**Why this priority**: String construction in suppressed log calls is wasteful but low-severity — it affects CPU and memory marginally. This is a clean-up item that improves overall code quality.

**Independent Test**: Can be tested by verifying that, under a high log level (e.g., ERROR only), DEBUG/INFO log calls do not allocate temporary strings.

**Acceptance Scenarios**:

1. **Given** the log level is set to ERROR, **When** a DEBUG-level log call is reached, **Then** no string formatting or allocation occurs.
2. **Given** the log level is set to INFO, **When** an INFO-level log call is reached, **Then** the message is formatted and logged normally.

---

### Edge Cases

- What happens when a peer is looked up that does not exist in the map? The system must handle missing keys gracefully (return not-found, not crash).
- What happens when the ban list is empty? Ban checks must return false (not banned) without errors.
- What happens when an RPC request arrives with a valid JSON-RPC envelope but an empty method name? The dispatch table must return a "method not found" error.
- What happens when PeerClient and PeerServer serialization paths diverge due to a merge conflict? The shared utility ensures a single source of truth, eliminating this class of error.
- What happens when code assumes a specific peer iteration order? The peer container does not guarantee ordering; callers and tests must not depend on insertion order.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST perform peer lookup, addition, and removal operations in amortized constant time regardless of total peer count (cap-triggered eviction may scan all entries).
- **FR-002**: System MUST store ban records in a dedicated constant-time container, separate from peers, so that ban-status checks complete in constant time regardless of the number of ban records.
- **FR-003**: System MUST dispatch RPC requests via a table-based lookup that maps method names to handler functions, replacing the current sequential comparison chain.
- **FR-004**: System MUST return a standard JSON-RPC "method not found" error (-32601) for unsupported method names.
- **FR-010**: Each extracted RPC handler MUST return a complete JSON response (success or error); the dispatcher MUST NOT wrap handlers in a catch-all exception handler.
- **FR-005**: System MUST consolidate the duplicated outbound packet serialization logic (currently in both PeerClient and PeerServer) into a single shared header-only template utility.
- **FR-006**: The shared serialization utility MUST produce wire-compatible output with the current format (no protocol break).
- **FR-007**: System MUST consolidate all duplicated test utility functions (block mining helpers, chain builders, temporary directory management) into the existing shared test helpers module, removing local copies from individual test files.
- **FR-008**: System MUST provide a mechanism to skip message formatting for log calls when the message would be suppressed by the current log level.
- **FR-009**: All existing tests MUST continue to pass after these changes (no behavioral regressions).

### Key Entities

- **PeerEntry**: Represents a connected or known peer. Key attributes: host, port, last-seen timestamp, connection state.
- **BanRecord**: Represents a banned peer. Key attributes: peer address, ban expiry time.
- **RPC Handler**: A callable that processes a single JSON-RPC method. Key attributes: method name, handler function.
- **PacketSerializer**: Shared utility for serializing outbound P2P packets. Used by both client and server networking components.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Peer lookup, add, remove, and ban-check operations complete in constant time regardless of peer count (verified by algorithmic analysis — O(1) vs former O(n)).
- **SC-002**: RPC method dispatch completes in constant time regardless of the number of registered methods (O(1) hash lookup vs former O(n) string comparisons).
- **SC-003**: The packet serialization logic exists in exactly one location in the codebase — zero duplication between PeerClient and PeerServer.
- **SC-004**: Zero local definitions of `mineTestBlock()`, `buildValidChain()`, or temporary directory helpers exist outside `TestHelpers.hpp`.
- **SC-005**: The full test suite passes with identical results before and after all changes.
- **SC-006**: Suppressed log calls produce zero heap allocations for message formatting (verified by code inspection or instrumentation).

## Assumptions

- The maximum peer count remains capped at 256 as configured today; the optimization is worthwhile even at this scale due to frequency of lookups.
- The existing P2P wire format is stable and will not change as part of this feature; the shared serialization utility preserves byte-level compatibility.
- `TestHelpers.hpp` already provides suitable signatures for the helpers being consolidated; minor signature adjustments may be needed to cover all four test files' usage patterns.
- The current logging utility (`logMessage()`) can be extended with a level-check macro or wrapper without changing its public API for existing callers.
- No new RPC methods are being added in this feature; the dispatch table refactor covers exactly the 20 methods that exist today.

## Clarifications

### Session 2026-04-13

- Q: What error-handling strategy should the extracted RPC handlers use? → A: Each handler returns a JSON result (success or error); the dispatcher does not catch exceptions from handlers. Handlers own their error responses.
- Q: Where should the shared packet serialization utility live? → A: Header-only free function template in a dedicated header file, since the function must be a template to accept any serializable type.
- Q: Should the refactored peer container preserve insertion order? → A: No, arbitrary order is acceptable. Tests that depend on peer ordering must be updated.
