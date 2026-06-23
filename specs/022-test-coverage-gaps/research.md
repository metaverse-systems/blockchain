# Research: Peer Disconnect Test Coverage

## Purpose

Resolve the last remaining medium-severity coverage gap from AUDIT.md §7.5: `Peer disconnect during propagation` — no existing tests exercise disconnect handlers during block propagation.

---

## Decision: Test relay callback exceptions via mock relay callbacks

**Rationale**: The `BlockPropagation` class accepts a `RelayCallback` (std::function) that is invoked after a valid block is appended. In production this callback calls `peer_manager.relay_block(block, exclude_key)` which in turn calls `PeerClient::send()` or `PeerServer::send_packet_public()`. Both can throw exceptions (network errors, closed sockets, etc.).

Tests should inject a relay callback that simulates exceptions for specific peers to verify:
- Block propagation continues to remaining peers (relay failure is caught)
- Node does not crash
- Exception is caught and logged

**Alternatives considered**:
- Integration tests with real network sockets: Too slow, flaky, and requires complex TLS setup
- Mock PeerClient/PeerServer: Would require extensive refactoring of PeerManager to accept mocks
- Mock relay callback: Lightweight, isolated, matches the existing test pattern in `block_propagation_tests.cpp`

---

## Decision: Test `on_peer_disconnected` and `on_inbound_disconnected` directly

**Rationale**: `PeerManager::on_peer_disconnected()` and `on_inbound_disconnected()` are public methods that can be invoked directly in tests. They manage:
- Outbound connection removal
- Error count increment
- Inbound session dedup check
- Reconnection scheduling
- Inbound count decrement

Testing these methods directly verifies the state transitions without requiring a running network.

**Alternatives considered**:
- Integration tests with real connections: Unnecessary complexity for unit-level state transitions
- Mocking PeerClient: Not needed since `on_peer_disconnected` only manipulates maps and peer entries

---

## Decision: Test reconnection scheduling and inbound dedup via PeerManager state inspection

**Rationale**: `PeerManager` exposes:
- `outbound_count()` — verify peers are removed from outbound connections
- `inbound_count()` — verify inbound count decrements
- `find_peer()` — verify error count increment
- `is_banned()` — verify banned peers are not reconnected
- `schedule_reconnect()` — public method to verify reconnection logic

Tests can set up peer entries with known UUIDs, simulate the disconnect, and assert state changes.

**Alternatives considered**:
- Mocking timer callbacks: Too complex for unit tests
- Integration tests: Unnecessarily slow

---

## Decision: Add tests to `block_propagation_tests.cpp` and `peer_manager_tests.cpp`

**Rationale**: Existing test files already contain the infrastructure:
- `block_propagation_tests.cpp` uses `MockBlockchain`, `ChainService`, `SyncStatus`, and mock relay callbacks
- `peer_manager_tests.cpp` uses `MockChunk`, `Blockchain`, `ChainService`, and real PeerManager instances

Adding disconnect tests to these files follows the existing pattern and minimizes new file creation.

**Alternatives considered**:
- New dedicated test file: Would fragment related tests and add build complexity
- Adding to integration tests: These are unit tests (no network), not integration tests

---

## Code Path Analysis

### Relay Callback Path (Production)
```
BlockPropagation::on_block_received()
  → relay_cb_(block, sender_key)        // RelayCallback = std::function<void(Block&, string&)>
    → PeerManager::relay_block()
      → PeerManager::send_to_peers()
        → PeerClient::send()            // Can throw on network error
        → PeerServer::send_packet_public() // Can throw on network error
```

### Disconnect Handler Path (Production)
```
PeerClient::handle_disconnect()
  → PeerManager::on_peer_disconnected()
    → outbound_connections_.erase()     // Remove from active pool
    → peer->error_count++               // Increment error count
    → (optional) schedule_reconnect()   // If not banned and no inbound session

PeerServer (inbound disconnect)
  → PeerManager::on_inbound_disconnected()
    → inbound_sessions_.erase()         // Remove session
    → inbound_count_--                  // Decrement count
```

### Reconnection Scheduling
```
PeerManager::schedule_reconnect()
  → backoff_state_[key] = {retry_after, attempts}
  → async_timer to call connect_to() after delay
```

---

## Test Scenarios

### Scenario 1: Relay callback throws exception for one peer
- **Setup**: MockBlockchain + BlockPropagation with relay callback that tracks calls and throws for a specific peer key
- **Action**: Receive a valid block
- **Assertions**: Block is appended, relay was called, exception for one peer doesn't prevent block append, no crash

### Scenario 2: Relay callback throws — propagation continues to other peers
- **Setup**: Relay callback that throws for peer A but succeeds for peer B
- **Action**: Receive a valid block
- **Assertions**: Relay called for both peers, peer A threw but peer B still received the call

### Scenario 3: Outbound peer disconnect increments error count
- **Setup**: PeerManager with a peer entry added
- **Action**: Call `on_peer_disconnected()`
- **Assertions**: Peer error_count incremented, peer removed from outbound connections

### Scenario 4: Outbound peer disconnect schedules reconnect (non-banned)
- **Setup**: PeerManager with non-banned peer
- **Action**: Call `on_peer_disconnected()`
- **Assertions**: Reconnection is scheduled (backoff_state contains peer)

### Scenario 5: Outbound peer disconnect skips reconnect if banned
- **Setup**: PeerManager with banned peer
- **Action**: Call `on_peer_disconnected()`
- **Assertions**: No reconnection scheduled

### Scenario 6: Outbound peer disconnect skips reconnect if inbound session exists
- **Setup**: PeerManager with peer having both outbound connection and inbound session (same UUID)
- **Action**: Call `on_peer_disconnected()`
- **Assertions**: No reconnection scheduled (inbound session prevents duplicate connection)

### Scenario 7: Inbound peer disconnect removes session and decrements count
- **Setup**: PeerManager with inbound session added via `on_inbound_connected()`
- **Action**: Call `on_inbound_disconnected()`
- **Assertions**: Inbound count decremented, session removed

### Scenario 8: Multiple relay failures don't crash node
- **Setup**: Relay callback that throws for all peers
- **Action**: Receive a valid block
- **Assertions**: Block is still appended, node doesn't crash

---

## Block Rate Considerations

The rate limiter allows 10 blocks/second per peer. Tests using relay callbacks with multiple peers should use different peer keys to avoid rate limiting interference.
