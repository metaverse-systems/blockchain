# Feature Specification: Peer Discovery & Management

**Feature Branch**: `004-peer-discovery`  
**Created**: 2026-04-10  
**Status**: Draft  
**Input**: User description: "Implement 004 — Peer Discovery & Management"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Seed Node Bootstrap (Priority: P1)

A node operator starts a new blockchain node for the first time and provides one or more seed node addresses. The node connects to seed nodes and obtains a list of other active peers in the network, then establishes connections to a subset of those peers automatically.

**Why this priority**: Without initial peer discovery, nodes cannot join the network. This is the fundamental capability that all other peer management features depend on.

**Independent Test**: Can be tested by starting two nodes — one as a seed and one as a joiner — and verifying the joiner discovers and connects to the seed, then receives a peer list.

**Acceptance Scenarios**:

1. **Given** a node is started with a seed node address configured, **When** the node starts up, **Then** it connects to the seed node and requests a list of known peers.
2. **Given** a seed node has knowledge of 5 other peers, **When** a new node requests the peer list, **Then** the new node receives addresses for all 5 peers.
3. **Given** all configured seed nodes are unreachable, **When** the node starts up, **Then** it logs an error and retries connection with exponential backoff.

---

### User Story 2 - Peer Exchange (Priority: P1)

Two connected nodes periodically share their known peer lists with each other so that newly joined nodes propagate through the network without every node needing to connect to the same seed.

**Why this priority**: Peer exchange (gossip) is essential for a decentralized network topology. Without it, the seed node becomes a single point of failure and a bottleneck.

**Independent Test**: Can be tested by starting three nodes (A, B, C) where A knows B and B knows C. After peer exchange, A should discover C without having been configured to know about C.

**Acceptance Scenarios**:

1. **Given** node A is connected to node B and node B is connected to node C, **When** peer exchange occurs between A and B, **Then** node A learns about node C's address.
2. **Given** a node receives a peer address via exchange, **When** that address is not already in its peer list, **Then** the node adds it and attempts to connect.
3. **Given** a node receives its own address in a peer exchange message, **When** processing the peer list, **Then** it discards its own address without error.

---

### User Story 3 - Connection Limits & Health (Priority: P2)

A node operator configures maximum inbound and outbound connection limits. The node respects those limits and replaces connections that trigger disconnect timeouts with other known peers.

**Why this priority**: Connection management prevents resource exhaustion and ensures stable network participation, but the network can function without limits at small scale.

**Independent Test**: Can be tested by configuring a node with a max outbound connection limit of 3, then presenting it with 10 available peers and verifying it connects to exactly 3.

**Acceptance Scenarios**:

1. **Given** a node has a maximum outbound connection limit of N, **When** more than N peers are discovered, **Then** the node connects to at most N outbound peers.
2. **Given** a node has a maximum inbound connection limit of M, **When** a new inbound connection arrives and M connections already exist, **Then** the new connection is rejected gracefully.
3. **Given** an established peer connection becomes unresponsive, **When** the health check detects the failure, **Then** the node closes the connection and attempts to connect to a different known peer.

---

### User Story 4 - Reconnection with Backoff (Priority: P2)

When a peer connection drops unexpectedly, the node automatically attempts to reconnect using exponential backoff to avoid overwhelming the target node or the network.

**Why this priority**: Automatic reconnection is critical for network resilience, but a node can still function if peers are manually re-added.

**Independent Test**: Can be tested by connecting two nodes, killing one, and observing the surviving node's reconnection attempts with increasing intervals.

**Acceptance Scenarios**:

1. **Given** a connected peer becomes unreachable, **When** the disconnection is detected, **Then** the node schedules a reconnection attempt after a base delay.
2. **Given** a reconnection attempt fails, **When** the node retries, **Then** the delay doubles (exponential backoff) up to a configured maximum delay.
3. **Given** a reconnection attempt succeeds, **When** the connection is re-established, **Then** the backoff timer resets to the base delay for that peer.

---

### User Story 5 - Manual Peer Management via RPC (Priority: P2)

A node operator uses RPC commands to manually add or remove peers and to inspect the current peer list. An option exists to disable automatic peer discovery entirely, so the operator has full manual control over which peers connect.

**Why this priority**: Manual control is important for private networks, testing, and debugging, but is not required for basic network participation.

**Independent Test**: Can be tested by starting a node with discovery disabled, adding a peer via RPC, verifying the connection is established, then removing it and verifying disconnection.

**Acceptance Scenarios**:

1. **Given** a running node, **When** the operator issues an "add peer" RPC command with a valid host and port, **Then** the node attempts to connect to that peer and adds it to the peer list.
2. **Given** a running node with connected peers, **When** the operator issues a "list peers" RPC command, **Then** the node returns the current peer list with connection status for each peer.
3. **Given** a running node with automatic discovery disabled, **When** no manual peer additions are made, **Then** the node does not initiate any outbound peer connections.

---

### User Story 6 - Peer Ban & Reputation (Priority: P3)

Misbehaving peers (sending invalid data, flooding connections, repeatedly disconnecting) are tracked and can be temporarily or permanently banned to protect the node from abuse.

**Why this priority**: Reputation management improves network health and security but is a hardening feature — the network can operate without it initially.

**Independent Test**: Can be tested by simulating a peer that sends malformed data repeatedly, then verifying the receiving node bans that peer's address.

**Acceptance Scenarios**:

1. **Given** a peer sends invalid or malformed messages, **When** the error count for that peer exceeds a configurable threshold, **Then** the node disconnects and bans the peer for a configurable duration.
2. **Given** a peer is banned, **When** that peer attempts to reconnect or is discovered via peer exchange, **Then** the connection is refused until the ban expires.
3. **Given** a node operator, **When** they issue an "unban peer" RPC command, **Then** the ban is lifted immediately regardless of the remaining duration.

---

### Edge Cases

- What happens when a node receives a peer exchange message containing hundreds of peers? Since nodes share their full peer list, the receiver enforces the 256-peer storage cap and evicts oldest-seen entries first when the limit is exceeded.
- What happens when two nodes simultaneously discover each other and both initiate connections? Duplicate connections are detected after TLS handshake by exchanging node UUIDs; the node with the lower UUID keeps its outbound connection and the other drops its outbound.
- What happens when a node's peer list file is corrupted on startup? The node should log a warning, discard the corrupted data, and start with an empty peer list (falling back to seed nodes).
- What happens when all known peers are unreachable? The node should continue retrying seed nodes with backoff and remain ready to accept inbound connections.
- What happens when peers are on different network interfaces (IPv4 vs IPv6)? The node should support both address families, consistent with the existing dual-stack configuration.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST support configuring one or more seed node addresses at startup.
- **FR-002**: System MUST request and receive a peer list from seed nodes upon initial connection.
- **FR-003**: System MUST implement a peer exchange protocol where connected peers periodically share their full known peer list (all non-banned addresses).
- **FR-004**: System MUST persist the known peer list to a JSON file in the blockchain data directory. The file MUST also contain a node UUID that is auto-generated on first run and stable across restarts.
- **FR-005**: System MUST support configurable maximum inbound and outbound connection limits.
- **FR-006**: System MUST automatically reconnect to dropped peers using exponential backoff with a configurable base delay and maximum delay.
- **FR-007**: System MUST expose RPC commands to add a peer, remove a peer, list peers, ban a peer, and unban a peer.
- **FR-008**: System MUST support a configuration option to disable automatic peer discovery, allowing only manual peer management via RPC.
- **FR-009**: System MUST track peer misbehavior and automatically ban peers that exceed a configurable error threshold.
- **FR-010**: System MUST detect and close duplicate connections to the same peer. When two nodes simultaneously connect to each other, the tie is broken by comparing node UUIDs — the node with the lower UUID keeps its outbound connection; the other drops its outbound connection.
- **FR-011**: System MUST filter out its own address from received peer lists.
- **FR-012**: System MUST cap the number of stored peer addresses at 256 to prevent unbounded memory growth. When the cap is reached, the oldest-seen entries are evicted first.
- **FR-013**: System MUST support both IPv4 and IPv6 peer addresses.
- **FR-014**: System MUST authenticate peers using the existing mutual TLS mechanism before participating in peer exchange.

### Key Entities

- **Peer**: Represents a known node in the network. Attributes include node UUID, address (host + port), connection state (connected, disconnected, banned), last seen time, error count, and ban expiry.
- **Node Identity**: A UUID auto-generated on first run and stored in the peer list JSON file. Used to identify the local node in peer exchange messages and to resolve duplicate connections.
- **Peer List**: The node's collection of known peers, both currently connected and previously seen. Persisted to disk.
- **Seed Node**: A well-known peer address configured at startup to bootstrap network entry.
- **Peer Exchange Message**: A protocol message carrying a list of peer addresses, exchanged periodically between connected peers.
- **Ban Record**: Tracks a banned peer's address, ban reason, and expiry time.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A new node joining the network discovers all reachable peers within 60 seconds of startup when provided with at least one reachable seed node.
- **SC-002**: A node that loses connectivity to a peer re-establishes the connection within 5 minutes of the peer becoming available again.
- **SC-003**: A 10-node network achieves full peer awareness (every node knows about every other node) within 3 minutes of the last node joining. The design should support up to 100 nodes without architectural changes.
- **SC-004**: Node operators can add and remove peers manually via RPC with immediate effect.
- **SC-005**: A node with connection limits configured never exceeds the configured inbound or outbound maximums.
- **SC-006**: Misbehaving peers are automatically banned within 30 seconds of exceeding the error threshold.
- **SC-007**: A node restarts and reconnects to previously known peers without manual intervention, using the persisted peer list.

## Clarifications

### Session 2026-04-10

- Q: Should peer exchange share all known peers or a random subset? → A: Share the full known peer list with every connected peer.
- Q: What is the maximum number of stored peer addresses? → A: 256 peers.
- Q: How is the peer list persisted and how are duplicate connections resolved? → A: JSON file with an auto-generated node UUID (created on first run); duplicate connections are resolved by comparing node UUIDs — the lower UUID keeps its outbound connection.
- Q: What is the target network scale for the peer discovery design? → A: 100 nodes. The design should work at this scale without changes.
- Q: Should new peer config use .env or a different format? → A: Introduce `config.json` to replace `.env` for all node configuration (TLS, ports, consensus, peer settings). Runtime peer state stays in a separate `peers.json`.

## Assumptions

- Nodes are identifiable by an auto-generated UUID stored in the peer list JSON file. This UUID is created on first run and persists across restarts. It is exchanged during peer protocol messages for duplicate connection resolution and peer tracking.
- The existing mutual TLS infrastructure is sufficient for peer authentication. No additional authentication layer is introduced.
- Seed node addresses are provided via configuration (environment variables or config file) and are assumed to be correct. Seed node discovery via DNS or other mechanisms is out of scope.
- Connection limits default to reasonable values (e.g., 8 outbound, 32 inbound) if not explicitly configured by the operator.
- The system is designed to operate correctly with up to 100 nodes. Larger deployments may require tuning of exchange intervals, connection limits, and peer storage caps.
- Peer exchange intervals default to 30 seconds. Operators can tune this via configuration.
- Ban durations default to 1 hour for automatic bans. Operators can configure the duration and error threshold.
- The peer list persistence format is implementation-defined but must survive unclean shutdowns (write-ahead or atomic replacement).
- Private network mode (discovery disabled) is a supported use case for testing and permissioned deployments.
