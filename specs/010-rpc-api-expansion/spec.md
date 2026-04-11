# Feature Specification: RPC API Expansion

**Feature Branch**: `010-rpc-api-expansion`  
**Created**: 2026-04-11  
**Status**: Draft  
**Input**: User description: "Implement 010 — RPC API Expansion: Add status, chain info, and introspection endpoints to the JSON-RPC server"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Operator Checks Node Health (Priority: P1)

A node operator wants to quickly determine whether their blockchain node is healthy — is it syncing, how tall is the chain, how many peers are connected, and what is the current mining difficulty. They call a single RPC method and receive a comprehensive status snapshot.

**Why this priority**: Operators need visibility into node state before any other introspection capability. Without a status endpoint, operators are blind to chain height, sync state, and connectivity — making troubleshooting impossible.

**Independent Test**: Can be fully tested by starting a node, calling the status endpoint, and verifying the response contains chain height, sync state, peer counts, and difficulty. Delivers immediate operational visibility.

**Acceptance Scenarios**:

1. **Given** a running node with blocks and connected peers, **When** the operator calls `getNodeStatus`, **Then** the response includes chain length, chunk count, sync state, current difficulty, peer counts (inbound and outbound), and the node's UUID.
2. **Given** a node that is actively syncing, **When** the operator calls `getNodeStatus`, **Then** the sync state field reflects that synchronization is in progress.
3. **Given** a freshly started node with no peers, **When** the operator calls `getNodeStatus`, **Then** the response shows zero peers and chain length of 1 (genesis block only).

---

### User Story 2 - Client Fetches a Range of Blocks (Priority: P2)

A client application needs to retrieve a contiguous range of blocks — for example, to display a block explorer page or to audit a section of the chain. Instead of making dozens of individual `getBlockByIndex` calls, the client requests an entire range in one call.

**Why this priority**: Batch retrieval is the highest-impact efficiency gain for clients. Block explorers, auditors, and sync tools all need range queries. Without this, clients must make N sequential RPC calls to fetch N blocks.

**Independent Test**: Can be fully tested by populating a chain with multiple blocks, requesting a range, and verifying the correct blocks are returned in order. Delivers immediate client usability improvement.

**Acceptance Scenarios**:

1. **Given** a chain with 50 blocks, **When** the client calls `getBlockRange` with start index 10 and end index 20, **Then** the response contains blocks 10 through 20 inclusive, in ascending index order.
2. **Given** a chain with 50 blocks, **When** the client calls `getBlockRange` with an end index beyond the chain length, **Then** the response returns blocks from the start index up to the last available block (no error).
3. **Given** a chain with 50 blocks, **When** the client calls `getBlockRange` with start index greater than end index, **Then** the response is an error indicating invalid range.
4. **Given** a chain with 50 blocks, **When** the client calls `getBlockRange` with a start index beyond the chain length, **Then** the response is an error indicating the start index is out of range.
5. **Given** a chain with 50 blocks, **When** the client calls `getBlockRange` with start index 0, end index 10, and `headersOnly` set to true, **Then** the response contains header-only block objects (no stream entries or full data) for blocks 0 through 10.

---

### User Story 3 - Operator Queries Chain Metrics Individually (Priority: P3)

An operator or monitoring script wants to query specific chain metrics — chain length or chunk count — as lightweight, fast calls for scripting and dashboards. These are simpler alternatives to the full `getNodeStatus` response.

**Why this priority**: Lightweight metric endpoints are essential for monitoring integrations and shell scripts that poll a single value. They complement the full status endpoint with minimal overhead.

**Independent Test**: Can be fully tested by calling each metric endpoint on a node with known state and verifying the returned value matches the expected count.

**Acceptance Scenarios**:

1. **Given** a chain with 150 blocks, **When** the operator calls `getChainLength`, **Then** the response is the integer 150.
2. **Given** a chain with 150 blocks distributed across 2 chunks, **When** the operator calls `getChunkCount`, **Then** the response is the integer 2.
3. **Given** a chain with only the genesis block, **When** the operator calls `getChainLength`, **Then** the response is the integer 1.

---

### Edge Cases

- What happens when `getBlockRange` is called with start = 0 and end = 0? Returns only the genesis block.
- What happens when `getBlockRange` is called with a range exceeding 1000 blocks? The system enforces a maximum range size to prevent excessive memory usage and response sizes.
- What happens when `getNodeStatus` is called during a chain recovery operation? The status reflects the current recovery state without blocking.
- What happens when `getChainLength` is called while blocks are being added? The response reflects the chain length at the moment the request is processed; concurrent additions may not be reflected.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST provide a `getNodeStatus` RPC method that returns a JSON object containing: chain length, chunk count, sync state (idle or syncing), current difficulty, inbound peer count, outbound peer count, and the node's UUID.
- **FR-002**: System MUST provide a `getBlockRange` RPC method that accepts `startIndex`, `endIndex`, and an optional `headersOnly` (boolean, default false) parameter. When `headersOnly` is false or omitted, it returns an array of full block objects (same format as `getBlockByIndex`). When `headersOnly` is true, it returns an array of header-only objects (same format as `getBlockHeader`).
- **FR-003**: System MUST enforce a maximum range size for `getBlockRange` to prevent excessive resource consumption. The limit is 1000 blocks per request.
- **FR-004**: System MUST provide a `getChainLength` RPC method that returns the total number of blocks in the chain as an integer.
- **FR-005**: System MUST provide a `getChunkCount` RPC method that returns the number of chunks as an integer.
- **FR-006**: `getBlockRange` MUST return blocks in ascending index order.
- **FR-007**: `getBlockRange` MUST clamp the end index to the last available block when the requested end index exceeds chain length (no error for this case).
- **FR-008**: `getBlockRange` MUST return error code `-32602` (invalid params) when the start index is greater than the end index.
- **FR-009**: `getBlockRange` MUST return error code `-32001` (not found) when the start index is beyond the chain length.
- **FR-010**: `getBlockRange` MUST return error code `-32602` (invalid params) when the requested range exceeds the maximum range size (1000 blocks).
- **FR-011**: All new RPC methods MUST follow the existing JSON-RPC 2.0 request/response format and error code conventions already established in the codebase.
- **FR-012**: All new RPC methods MUST validate their parameters and return appropriate error responses (code -32602) for missing or invalid parameters.

### Key Entities

- **NodeStatus**: Composite view of node health — chain length, chunk count, sync state, difficulty, peer counts, node UUID.
- **BlockRange**: An ordered sequence of blocks identified by start and end index, subject to a maximum size constraint.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Operators can determine full node health (chain height, sync state, peer connectivity, difficulty) with a single request.
- **SC-002**: Clients can retrieve up to 1000 contiguous blocks in a single request instead of making individual calls.
- **SC-003**: Monitoring scripts can poll chain length and chunk count via dedicated lightweight endpoints.
- **SC-004**: All new endpoints return well-formed JSON-RPC 2.0 responses with appropriate error codes for invalid inputs.
- **SC-005**: All new endpoints are covered by automated tests verifying correct behavior for normal inputs, boundary conditions, and error cases.

## Clarifications

### Session 2026-04-11

- Q: Should `getBlockRange` support an optional headers-only mode? → A: Yes, add an optional `headersOnly` boolean parameter; when true, return header-only objects (same format as `getBlockHeader`).
- Q: Should `getNodeStatus` expose the node's software/protocol version? → A: No, deferred to spec 016 (Monitoring, Metrics & Health).
- Q: Should `getBlockRange` errors use new dedicated error codes or reuse existing ones? → A: Reuse existing codes — `-32602` for invalid params (start > end, range too large), `-32001` for start beyond chain.

## Assumptions

- The existing JSON-RPC 2.0 server infrastructure (TLS, newline-delimited messages, error code conventions) is reused as-is; no changes to the transport layer are needed.
- Peer management endpoints (`addPeer`, `removePeer`, `listPeers`, `banPeer`, `unbanPeer`) are already implemented and do not need to be added — this spec focuses on the remaining introspection and range-query gaps.
- The `getBlockRange` maximum of 1000 blocks is a reasonable default based on typical block sizes in this system; it can be adjusted in a future configuration spec if needed.
- The `getNodeStatus` endpoint is read-only and does not modify any state.
- All data needed for the new endpoints is already accessible via existing `Blockchain`, `PeerManager`, and `SyncState` interfaces — no new data collection is required.
