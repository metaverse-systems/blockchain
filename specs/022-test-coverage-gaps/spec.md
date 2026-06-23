# Feature Specification: Peer Disconnect Test Coverage

**Feature Branch**: `022-test-coverage-gaps`  
**Created**: 2026-06-23  
**Status**: Draft  
**Input**: User description: "Address Issue 7.5"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Test peer disconnect during outbound block propagation (Priority: P1)

When a node is propagating a newly received block to its connected peers, one of those peers may disconnect mid-relay. The system should handle this gracefully: the relay failure should not crash the node, the block should still be relayed to remaining peers, and the disconnected peer should be tracked appropriately (error count incremented, reconnection scheduled).

**Why this priority**: This is the last remaining medium-severity coverage gap identified in the code audit (§7.5). Without this test, regressions in disconnect handling during propagation go undetected.

**Independent Test**: Can be fully tested by simulating a relay failure to one peer during block propagation, then verifying that relay failures are caught, remaining peers still receive the block, and the disconnect handler updates peer state correctly.

**Acceptance Scenarios**:

1. **Given** a node has multiple connected peers and receives a valid block, **When** relaying to one peer throws an exception (simulating disconnect), **Then** the block is still relayed to all other peers without crash
2. **Given** a peer disconnects during block propagation, **When** the disconnect is processed, **Then** the peer's error count is incremented and the peer is removed from the active connection pool
3. **Given** a peer disconnects during propagation and is not banned, **When** the disconnect is processed, **Then** a reconnection attempt is scheduled for that peer
4. **Given** a relay operation fails with an exception during propagation, **When** the exception is caught, **Then** the block propagation continues normally and does not terminate the node

---

### User Story 2 - Test inbound peer disconnect during block reception (Priority: P2)

When an inbound peer connection drops while the node is processing a block received from that peer, the node should clean up the inbound session state and continue operating without the disconnected peer.

**Why this priority**: Lower risk than outbound disconnect (inbound disconnection path is simpler), but still required for complete coverage of the disconnect-during-propagation scenario.

**Independent Test**: Can be tested by verifying that the inbound disconnect handler correctly removes the peer session, decrements the inbound connection count, and does not affect ongoing chain state or block processing.

**Acceptance Scenarios**:

1. **Given** an inbound peer is connected and has sent a block, **When** the peer disconnects, **Then** the inbound session is removed and the inbound connection count is decremented
2. **Given** an inbound peer disconnects, **When** the disconnect is processed, **Then** the node continues to accept blocks from other peers without disruption

---

### Edge Cases

- What happens when the relay callback throws an exception (e.g., network error during send)?
- How does the system handle a peer disconnect when all peers disconnect simultaneously during propagation?
- What happens if a peer disconnects but an inbound session from the same node is still alive (dedup scenario)?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Tests MUST verify that block propagation relay failures (simulated peer disconnect) do not crash the node or terminate block propagation
- **FR-002**: Tests MUST verify that outbound peer disconnect correctly increments the peer's error count and removes the peer from the active connection pool
- **FR-003**: Tests MUST verify that outbound peer disconnect schedules a reconnection attempt for non-banned peers
- **FR-004**: Tests MUST verify that inbound peer disconnect removes the session and decrements the inbound connection count
- **FR-005**: Tests MUST verify that block propagation continues to remaining peers when one peer disconnects mid-relay
- **FR-006**: Tests MUST verify that when a peer disconnects, an existing inbound session from the same node prevents an unnecessary reconnection attempt

### Key Entities

- **Peer State**: Tracks peer information including error count, last seen timestamp, connection status, and node identifier
- **Block Propagation**: Handles block reception, validation, deduplication, and relay to connected peers
- **Peer Manager**: Manages peer lifecycle including connection, disconnection, reconnection scheduling, and inbound session tracking

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All medium-severity coverage gaps listed in AUDIT.md §7.5 are resolved (0 remaining open items)
- **SC-002**: New tests achieve 100% pass rate across 10 consecutive executions (no flakes)
- **SC-003**: Test coverage for peer disconnect handlers and relay exception handling increases from 0 assertions to at least 5 meaningful assertions per scenario
- **SC-004**: All existing tests continue to pass without modification

## Assumptions

- The existing relay callback signature and exception behavior remain unchanged
- The existing disconnect handlers are correct and need test coverage (not bug fixes)
- Tests will use mock relay callbacks that simulate disconnect scenarios rather than requiring actual network connections
- Existing mock blockchain and test helper utilities are sufficient for test construction
- This feature adds test coverage only; no production code changes are expected unless a bug is discovered during testing
