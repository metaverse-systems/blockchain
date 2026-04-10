# Feature Specification: Chain Synchronization

**Feature Branch**: `003-chain-sync`  
**Created**: 2026-04-10  
**Status**: Draft  
**Input**: User description: "Implement 003 — Chain Synchronization from roadmap"

## Clarifications

### Session 2026-04-10

- Q: How should a syncing node decide between two chains of the same length but different content (a fork)? → A: Longest chain only; if equal length, keep the local chain. Deeper fork resolution is deferred to consensus spec 002.
- Q: Should validation rejection be per-chunk or for the entire sync? → A: Per-chunk. Reject only the invalid chunk and re-request it; keep previously validated chunks.
- Q: What timeout should apply when waiting for a peer's chunk response? → A: 60 seconds per chunk.
- Q: Should a node allow local block creation via addBlock RPC while syncing? → A: No. Reject addBlock calls during sync with an error indicating the node is syncing.
- Q: Should sync also be triggerable on demand via an RPC command? → A: Yes. Add an RPC method to manually trigger sync in addition to automatic on-connect sync.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - New Node Joins and Downloads Chain (Priority: P1)

A node operator starts a new blockchain node and connects it to an existing peer that already has an established chain. The new node automatically detects that it is behind and requests the full chain from the peer. Once the download completes, the new node has an identical copy of the chain and can participate in the network.

**Why this priority**: Without initial chain sync, new nodes cannot join the network at all. This is the fundamental capability that makes the blockchain distributed.

**Independent Test**: Start a node with an existing chain of multiple blocks, connect a fresh node to it, and verify the fresh node ends up with the same chain (identical block count, hashes, and data).

**Acceptance Scenarios**:

1. **Given** a peer node with a chain of 50 blocks, **When** a new node with only a genesis block connects, **Then** the new node downloads all 50 blocks and its chain matches the peer's chain exactly.
2. **Given** a peer node with a chain spanning multiple chunks, **When** a new node connects, **Then** the new node receives and reconstructs all chunks correctly.
3. **Given** a new node connecting to a peer, **When** the sync begins, **Then** the new node continues to accept RPC queries on already-synced blocks while sync is still in progress.

---

### User Story 2 - Node Recovers After Downtime (Priority: P2)

A node operator restarts a node that was offline for a period during which new blocks were added to the network. Upon reconnecting, the node detects that its chain is shorter than the peer's chain and downloads only the missing blocks to catch up, rather than re-downloading the entire chain.

**Why this priority**: Nodes going offline and coming back is a normal operational scenario. Efficient catch-up sync avoids unnecessary data transfer and reduces recovery time.

**Independent Test**: Start two nodes in sync, add blocks to one while the other is stopped, restart the stopped node and verify it downloads only the missing blocks.

**Acceptance Scenarios**:

1. **Given** a node with 200 blocks reconnects to a peer with 250 blocks, **When** sync initiates, **Then** only blocks 201–250 are transferred.
2. **Given** a node that was offline, **When** it reconnects, **Then** it automatically initiates sync without manual intervention.

---

### User Story 3 - Node Rejects Invalid Chain Data (Priority: P2)

A node receives chain data from a peer during synchronization. The node validates every block it receives using the existing validation rules (index continuity, hash linkage, proof-of-work difficulty). If any block fails validation, the node rejects the entire sync response from that peer and logs the failure.

**Why this priority**: Accepting unvalidated chain data would undermine the integrity of the blockchain. Security validation during sync is essential for trust.

**Independent Test**: Send a sync response containing a block with a tampered hash to a syncing node and verify the node rejects the response and logs an error.

**Acceptance Scenarios**:

1. **Given** a peer sends a chain where one block has an invalid hash, **When** the syncing node validates the response, **Then** the node rejects the entire batch and does not modify its local chain.
2. **Given** a peer sends a chain where a block does not meet the required proof-of-work difficulty, **When** the syncing node validates it, **Then** the node rejects the batch.
3. **Given** a peer sends a valid chain that is longer than the local chain, **When** the syncing node validates it, **Then** the node accepts and replaces its local chain.

---

### User Story 4 - Sync Handles Network Interruptions Gracefully (Priority: P3)

During chain synchronization, the network connection between two nodes drops. The syncing node detects the disconnection, preserves any already-validated data it has received, and retries sync when the connection is re-established.

**Why this priority**: Network reliability cannot be guaranteed. Graceful handling of interruptions prevents data corruption and unnecessary re-transfers.

**Independent Test**: Initiate a sync between two nodes, sever the connection mid-transfer, reconnect the nodes, and verify the sync completes without data loss or corruption.

**Acceptance Scenarios**:

1. **Given** a sync is in progress, **When** the connection drops, **Then** the syncing node does not corrupt its local chain state.
2. **Given** a sync was interrupted, **When** the connection is re-established, **Then** the node resumes sync from where it left off rather than restarting from the beginning.

---

### Edge Cases

- What happens when two nodes connect and both have the same chain length? The node keeps its local chain. Even if block content differs (a fork), equal-length chains are not replaced; fork resolution beyond longest-chain is deferred to consensus spec 002.
- What happens when a node receives a chain that is shorter than or equal in length to its own? The node ignores it and retains its local chain.
- What happens when the chain spans more chunks than can fit in memory at once? The sync transfers and persists chunks incrementally so that the full chain never needs to reside entirely in memory.
- What happens when multiple new nodes try to sync from the same peer simultaneously? The peer handles concurrent sync requests independently without blocking.
- What happens when a sync response arrives for a chain the node has already moved past (stale response)? The node discards stale sync responses.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST implement the `BLOCKCHAIN_QUERY` packet type so that a node can request chain information from a connected peer.
- **FR-002**: System MUST implement the `BLOCKCHAIN_RESPONSE` packet type so that a node can send chain data in response to a sync request.
- **FR-003**: System MUST include the requesting node's current chain length in the `BLOCKCHAIN_QUERY` message so the responding peer can determine what data to send.
- **FR-004**: System MUST transfer chain data at the chunk level (groups of blocks) rather than individual blocks, to align with the existing chunk-based storage model.
- **FR-005**: System MUST validate every received block against existing validation rules (`isValidNewBlock`) before incorporating it into the local chain.
- **FR-006**: System MUST reject a chunk if any block within it fails validation. Previously validated and persisted chunks are retained; only the invalid chunk is discarded and may be re-requested.
- **FR-007**: System MUST automatically initiate chain sync upon connecting to a peer, without requiring manual intervention.
- **FR-008**: System MUST support incremental sync — only requesting blocks the node does not already have, rather than the full chain.
- **FR-014**: System MUST use a longest-chain rule for sync decisions: only replace the local chain when the peer's chain is strictly longer. Equal-length chains always keep the local version.
- **FR-015**: System MUST apply a 60-second timeout when waiting for a peer to respond to a chunk request. If the timeout expires, the node treats the peer as unresponsive and aborts the sync attempt with that peer.
- **FR-009**: System MUST persist synced chunks to disk as they are received and validated, so that progress is not lost on interruption.
- **FR-010**: System MUST handle connection failures during sync gracefully, preserving already-validated local state.
- **FR-011**: System MUST allow the node to continue serving read-only RPC requests (e.g., `getBlockByIndex`, `getBlocksByKeys`) during synchronization.
- **FR-016**: System MUST reject `addBlock` RPC calls while a sync operation is in progress, returning an error that indicates the node is currently syncing.
- **FR-017**: System MUST expose an RPC method that allows operators to manually trigger a chain sync on demand, in addition to the automatic on-connect sync.
- **FR-012**: System MUST log sync progress including blocks received, validation outcomes, and any errors encountered.
- **FR-013**: System MUST handle concurrent sync requests from multiple peers without data corruption.

### Key Entities

- **Sync Request (BLOCKCHAIN_QUERY)**: A message sent by a node to a peer indicating "I have blocks up to height N, send me what I'm missing." Contains the sender's current chain height.
- **Sync Response (BLOCKCHAIN_RESPONSE)**: A message sent by a peer containing a batch of blocks (chunk-aligned) that the requesting node is missing. Contains the block data and the sender's total chain height.
- **Chunk**: An existing entity — a group of up to 100 blocks stored together. Sync transfers align with chunk boundaries for efficiency.
- **Block**: An existing entity — the fundamental unit of the chain, validated individually during sync.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A new node joining a network with 1,000 blocks completes initial sync and has a fully verified chain within 60 seconds on a local network.
- **SC-002**: A node that was offline while 100 new blocks were added catches up within 10 seconds of reconnecting.
- **SC-003**: 100% of blocks received during sync are validated before being added to the local chain — no unvalidated blocks are ever persisted.
- **SC-004**: A sync interrupted at any point does not leave the syncing node's chain in a corrupted or inconsistent state.
- **SC-005**: A node remains responsive to RPC queries during an active sync operation, with queries completing within normal response times.
- **SC-006**: A peer node can handle at least 3 concurrent sync requests from different nodes without failure.

## Assumptions

- Nodes are already able to establish TLS-secured P2P connections to each other (implemented in existing codebase).
- The existing `BLOCKCHAIN_QUERY` and `BLOCKCHAIN_RESPONSE` packet types in `PacketHeader.hpp` define the wire-level message type identifiers to be used.
- The existing `replaceChain` and `isValidNewBlock` methods provide the core validation logic that sync will build upon.
- Chunk-based storage (100 blocks per chunk) is the existing persistence model and sync will align with this granularity.
- Peer discovery is out of scope — nodes are assumed to know at least one peer's address to connect to (peer discovery is roadmap item 004).
- Block propagation of newly mined blocks is out of scope — this spec covers only catch-up synchronization of historical chain data (block propagation is roadmap item 005).
- The consensus mechanism (roadmap item 002) defines the validation rules that sync must respect; this spec assumes those rules exist and are available via `isValidNewBlock`.
