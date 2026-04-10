# Feature Specification: Block Propagation & Validation on Receipt

**Feature Branch**: `005-block-propagation`  
**Created**: 2026-04-10  
**Status**: Draft  
**Input**: User description: "Implement 005 — Block Propagation & Validation on Receipt"

## Clarifications

### Session 2026-04-10

- Q: When a block arrives whose prevHash doesn't match the current chain tip, should the node discard it or defer it? → A: Defer — hold in a bounded pending pool; re-evaluate when the missing predecessor arrives.
- Q: What identifier should the recent-block cache use as the deduplication key? → A: Block hash.
- Q: Should the node enforce a per-peer rate limit on incoming blocks to prevent flooding? → A: Yes — per-peer rate limit; excess blocks are dropped and count as errors.
- Q: Should individually propagated blocks be processed while a bulk chain sync is in progress? → A: Queue during sync; process after sync completes.
- Q: What is the maximum sustained block propagation rate the node should support? → A: 10 blocks/second.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Locally Created Blocks Are Broadcast to Peers (Priority: P1)

As a node operator, when my node creates a new block (via the add-block command), I want that block to be automatically sent to all connected peers so that the network stays in sync without manual intervention.

**Why this priority**: Without outbound block broadcast, locally mined blocks remain isolated on a single node, making the network fragmented. This is the fundamental requirement for a distributed blockchain.

**Independent Test**: Create a block on Node A while Node B is connected. Verify Node B receives the block within a reasonable time.

**Acceptance Scenarios**:

1. **Given** Node A and Node B are connected, **When** Node A creates a new block, **Then** Node B receives the block automatically.
2. **Given** Node A is connected to Nodes B, C, and D, **When** Node A creates a new block, **Then** all three peers receive the block.
3. **Given** Node A has no connected peers, **When** Node A creates a new block, **Then** the block is stored locally without errors and will be synced when peers connect later.

---

### User Story 2 - Received Blocks Are Validated Before Acceptance (Priority: P1)

As a node operator, when my node receives a block from a peer, I want the node to validate that block against the consensus rules before adding it to the chain so that only legitimate blocks become part of my local chain.

**Why this priority**: Accepting unvalidated blocks would compromise chain integrity. This is equally critical to broadcast — without validation, propagation would be harmful.

**Independent Test**: Send a valid block and an invalid block (e.g., tampered hash) to a node. Verify the valid one is accepted and the invalid one is rejected.

**Acceptance Scenarios**:

1. **Given** a node receives a block with correct index, previous hash, hash integrity, and difficulty, **When** the node validates it, **Then** the block is appended to the local chain.
2. **Given** a node receives a block with an incorrect previous hash, **When** the node validates it, **Then** the block is rejected and not added to the chain.
3. **Given** a node receives a block with a tampered hash (hash doesn't match computed hash), **When** the node validates it, **Then** the block is rejected.
4. **Given** a node receives a block with insufficient proof-of-work difficulty, **When** the node validates it, **Then** the block is rejected.
5. **Given** a node receives a block whose index does not follow the current chain tip, **When** the node validates it, **Then** the block is rejected.

---

### User Story 3 - Valid Blocks Are Relayed to Other Peers (Priority: P2)

As a network participant, when my node receives and validates a block from one peer, I want it to forward that block to all other connected peers so that blocks propagate across the entire network, not just to direct neighbors.

**Why this priority**: Without relay, block propagation is limited to direct connections. Multi-hop relay is essential for network-wide consistency but depends on validation (P1) being in place first.

**Independent Test**: Connect three nodes in a line (A→B→C). Create a block on A. Verify it reaches C via B's relay.

**Acceptance Scenarios**:

1. **Given** Node B is connected to Nodes A and C, **When** Node B receives a valid block from Node A, **Then** Node B forwards the block to Node C.
2. **Given** Node B receives a valid block from Node A, **When** Node B relays it, **Then** Node B does NOT send the block back to Node A (the originator).
3. **Given** Node B receives an invalid block from Node A, **When** validation fails, **Then** Node B does NOT relay the block to any peer.

---

### User Story 4 - Duplicate Blocks Are Suppressed (Priority: P2)

As a node operator, I want my node to recognize blocks it has already seen and discard duplicates so that the network does not waste bandwidth or processing on redundant messages.

**Why this priority**: In a mesh topology, a node may receive the same block from multiple peers. Without deduplication, this creates unnecessary load and potential double-appends. Important for network health but not for basic correctness.

**Independent Test**: Send the same valid block to a node from two different peers. Verify the block is added only once and the second copy is silently discarded.

**Acceptance Scenarios**:

1. **Given** a node has already accepted block N, **When** it receives block N again from another peer, **Then** the duplicate is discarded without error.
2. **Given** a node receives the same block from three peers nearly simultaneously, **When** processing completes, **Then** the block appears exactly once in the chain.
3. **Given** a node discards a duplicate block, **When** the discard occurs, **Then** the node does NOT relay the duplicate to other peers.

---

### User Story 5 - Misbehaving Peers Are Penalized (Priority: P3)

As a node operator, I want my node to track peers that repeatedly send invalid blocks and penalize them so that the network is resilient against malicious or buggy peers.

**Why this priority**: Defense against bad actors. Important for production robustness but not required for basic block propagation functionality.

**Independent Test**: Send multiple invalid blocks from a peer. Verify the peer's error count increases and that the peer is eventually disconnected or banned after exceeding the threshold.

**Acceptance Scenarios**:

1. **Given** a peer sends an invalid block, **When** validation fails, **Then** the node increments the error count for that peer.
2. **Given** a peer's error count reaches the configured threshold, **When** the threshold is exceeded, **Then** the peer is banned per existing ban policy.

---

### Edge Cases

- What happens when a received block is valid but refers to a previous block the node hasn't seen yet (gap in the chain)? *Resolved: defer in bounded pending pool (see FR-009).*
- What happens when two valid blocks arrive for the same index (fork scenario)?
- What happens when a block arrives during an active chain sync operation? *Resolved: queue and process after sync completes (see FR-012).*
- How does the node handle blocks that arrive out of order?
- What happens if the node's storage is full when a valid block is received?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST broadcast newly created blocks to all currently connected peers immediately after the block is added to the local chain.
- **FR-002**: System MUST validate every block received from a peer against all consensus rules (index sequence, previous hash linkage, hash integrity, and proof-of-work difficulty) before accepting it.
- **FR-003**: System MUST append validated blocks to the local chain after successful validation.
- **FR-004**: System MUST relay validated blocks to all connected peers except the peer that sent the block.
- **FR-005**: System MUST maintain a recent-block cache keyed by block hash to detect and discard duplicate blocks without re-processing.
- **FR-006**: System MUST silently discard duplicate blocks (already in chain or already seen) without treating them as errors.
- **FR-007**: System MUST increment a peer's error count when that peer sends an invalid block.
- **FR-008**: System MUST reject (discard without appending) any block that fails validation and MUST NOT relay rejected blocks.
- **FR-009**: System MUST hold received blocks whose prevHash does not match the current chain tip in a bounded pending pool, and re-evaluate them when the missing predecessor arrives or is synced. Blocks that remain unresolved after the pool reaches capacity or a timeout period MUST be evicted.
- **FR-010**: System MUST NOT broadcast a block back to the peer that originally sent it.
- **FR-011**: System MUST enforce a per-peer rate limit on inbound BLOCK packets. Blocks exceeding the configured rate MUST be dropped and MUST increment the sending peer's error count.
- **FR-012**: System MUST queue individually propagated blocks that arrive while a chain sync is in progress, and process the queue after sync completes. The queue MUST be bounded to prevent memory exhaustion.

### Key Entities

- **Block**: The fundamental unit of data on the chain — contains an index, timestamp, data payload, previous hash, hash, nonce, and difficulty.
- **Peer Connection**: A live connection to another node through which blocks are sent and received.
- **Recent-Block Cache**: A bounded set of recently seen block hashes used for deduplication.
- **Pending Block Pool**: A bounded buffer holding blocks whose predecessor has not yet been received, awaiting re-evaluation when the gap is filled.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A block created on any node reaches all connected peers within 5 seconds under normal network conditions.
- **SC-002**: 100% of invalid blocks (tampered hash, wrong index, insufficient difficulty) are rejected and never added to the chain.
- **SC-003**: A block originating at one node propagates across a 3-node chain (A→B→C) within 10 seconds.
- **SC-004**: Duplicate blocks received from multiple peers result in exactly one chain entry.
- **SC-005**: A peer that sends 5 or more invalid blocks is penalized according to the existing ban policy.
- **SC-006**: Block propagation does not interfere with ongoing chain synchronization — both can operate without data corruption.
- **SC-007**: The node sustains processing of at least 10 propagated blocks per second without degradation under normal conditions.

## Assumptions

- Peer-to-peer connectivity and the peer management layer (from spec 004) are operational and reliable.
- The consensus mechanism (from spec 002) is in place, providing the validation rules that received blocks are checked against.
- Chain synchronization (from spec 003) is functional and handles bulk block transfer; this feature covers individual real-time block relay only.
- The existing block serialization format is used for network transmission — no new wire format is introduced.
- Nodes operate in a trusted-TLS environment where peer identity is established at connection time.
- Fork resolution (choosing between competing chain tips) is out of scope for this feature and will be addressed in a future spec if needed. The node applies a simple rule: reject a block that doesn't extend the current tip.
- The recent-block cache has a reasonable bounded size (e.g., last few hundred block identifiers) and does not need to persist across restarts.
