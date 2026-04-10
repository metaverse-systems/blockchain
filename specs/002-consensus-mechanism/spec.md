# Feature Specification: Consensus Mechanism

**Feature Branch**: `002-consensus-mechanism`  
**Created**: 2026-04-10  
**Status**: Draft  
**Input**: User description: "Implement spec 002 — Consensus Mechanism. No consensus algorithm exists. Blocks are added locally without any distributed agreement. Implement a consensus protocol so that nodes agree on the canonical chain."

**Use Case Context**: This blockchain is designed for personal use — users back up data across multiple machines they own. Networks will have a small number of trusted peers (typically 2–10 nodes), not a large public network. Consensus serves primarily as tamper-evidence and consistency assurance between a user's own devices, not as defense against adversarial miners.

## Clarifications

### Session 2026-04-10

- Q: Should there be a maximum mining time limit, and what happens when exceeded? → A: Configurable timeout (e.g., 30s default); returns an error if mining doesn't complete in time.
- Q: Which difficulty representation model — leading zero bits or 256-bit target threshold? → A: Leading zero bits count; difficulty is an integer N and the hash must have N leading zero bits.
- Q: What should the initial/genesis difficulty be? → A: Difficulty 1 (1 leading zero bit); trivially easy, allowing the adjustment algorithm to ramp up naturally.
- Q: What maximum future timestamp drift should be allowed? → A: 120 seconds (2 minutes); tolerates typical clock drift while preventing timestamp manipulation.
- Q: Should there be a maximum reorganization depth? → A: Configurable max reorg depth (default 100 blocks); reject chains requiring deeper reorganization.
- Q: (Context provided) What is the intended use case? → A: Personal blockchains for data backup between a user's own machines; small trusted peer networks (2–10 nodes).

## User Scenarios & Testing

### User Story 1 - Node Validates Blocks Before Acceptance (Priority: P1)

As a node operator, I want my node to validate incoming blocks against consensus rules before adding them to the chain, so that only legitimately produced blocks become part of the canonical chain.

**Why this priority**: Without block validation against consensus rules, any node can forge blocks and corrupt the chain. This is the foundational requirement that all other consensus behavior depends on.

**Independent Test**: Can be fully tested by submitting blocks with varying difficulty proofs to a single node and verifying that only blocks meeting the difficulty target are accepted.

**Acceptance Scenarios**:

1. **Given** a node running with consensus enabled, **When** a block with a valid proof-of-work is submitted, **Then** the node accepts and appends the block to the chain.
2. **Given** a node running with consensus enabled, **When** a block without a valid proof-of-work is submitted, **Then** the node rejects the block and returns an error indicating invalid proof.
3. **Given** a node running with consensus enabled, **When** a block references an incorrect previous hash, **Then** the node rejects the block regardless of proof-of-work validity.

---

### User Story 2 - Node Mines Blocks with Proof-of-Work (Priority: P1)

As a node operator adding data to the blockchain, I want the node to compute a valid proof-of-work when creating a new block, so that the block will be accepted by all other nodes in the network.

**Why this priority**: Block creation is the primary workflow for adding data. Without proof-of-work computation, locally created blocks would be rejected by consensus-aware peers.

**Independent Test**: Can be fully tested by calling the addBlock operation and verifying the returned block contains a valid nonce and hash meeting the difficulty target.

**Acceptance Scenarios**:

1. **Given** a node with consensus enabled, **When** `addBlock` is called with valid data, **Then** the node computes a nonce such that the block's hash meets the current difficulty target.
2. **Given** a node mining a block, **When** the proof-of-work computation completes, **Then** the resulting block hash has at least the required number of leading zero bits.
3. **Given** a node mining a block, **When** the block is successfully mined, **Then** the block includes the nonce used and the timestamp reflects when mining completed.

---

### User Story 3 - Network Resolves Competing Chains (Priority: P2)

As a network participant, I want all nodes to converge on the same canonical chain when forks occur, so that the network maintains a single consistent ledger.

**Why this priority**: Forks are inevitable in a distributed system. Without a fork resolution rule, nodes will permanently diverge. This is essential for distributed operation but depends on validation (P1) being in place first.

**Independent Test**: Can be tested by creating two nodes with divergent chains of different lengths and verifying that the shorter chain is replaced by the longer valid chain.

**Acceptance Scenarios**:

1. **Given** a node with a chain of length N, **When** it receives a valid chain of length N+M from a peer, **Then** the node replaces its chain with the longer chain.
2. **Given** a node with a chain of length N, **When** it receives a valid chain of length N-1 from a peer, **Then** the node keeps its current chain.
3. **Given** a node with a chain of length N, **When** it receives a longer chain containing an invalid block, **Then** the node rejects the entire chain and keeps its own.

---

### User Story 4 - Difficulty Adjusts Over Time (Priority: P3)

As a network participant, I want the mining difficulty to adjust automatically based on block production rate, so that blocks are produced at a predictable interval regardless of changes in total network mining power.

**Why this priority**: A fixed difficulty either becomes trivially easy or impossibly hard as network hashrate changes. Adjustment is needed for long-term viability but is not required for initial consensus operation.

**Independent Test**: Can be tested by mining a sequence of blocks with artificially fast timestamps and verifying the difficulty increases, then mining with slow timestamps and verifying it decreases.

**Acceptance Scenarios**:

1. **Given** a configured target block interval, **When** the last N blocks were mined faster than the target, **Then** the difficulty increases for the next period.
2. **Given** a configured target block interval, **When** the last N blocks were mined slower than the target, **Then** the difficulty decreases for the next period.
3. **Given** difficulty adjustment is triggered, **When** the new difficulty is calculated, **Then** it does not change by more than a configured maximum adjustment factor.

---

### Edge Cases

- What happens when a block's timestamp is significantly in the future relative to the node's clock?
- How does the system handle a block with a nonce that overflows the numeric range during mining?
- What happens when two blocks at the same height arrive nearly simultaneously (natural fork)?
- How does the node behave when difficulty is at the minimum and blocks are still being produced too slowly?
- What happens when the genesis block is validated — does it follow the same consensus rules or is it exempt?
- How does the system handle a chain reorganization that affects blocks already queried by users? Reorganizations beyond the configurable max depth (default 100 blocks) are rejected.
- What happens when mining times out — is the partial work discarded or resumable?

## Requirements

### Functional Requirements

- **FR-001**: System MUST validate each new block's proof-of-work against the current difficulty target before accepting it into the chain.
- **FR-002**: System MUST compute a valid proof-of-work (nonce) when creating a new block locally, such that the block's hash meets the difficulty target.
- **FR-003**: System MUST include a nonce field and a difficulty field in each block's structure.
- **FR-004**: System MUST adopt the longest valid chain rule: when presented with a competing chain that is longer and fully valid, the node replaces its local chain, provided the reorganization depth does not exceed the configured maximum.
- **FR-005**: System MUST reject any competing chain that contains one or more invalid blocks (incorrect hash, invalid proof-of-work, broken linkage).
- **FR-006**: System MUST exempt the genesis block from proof-of-work requirements while still validating its structural integrity.
- **FR-007**: System MUST adjust mining difficulty periodically based on the rate of recent block production relative to a target block interval.
- **FR-008**: System MUST cap difficulty adjustments to prevent extreme changes in a single adjustment period (maximum adjustment factor).
- **FR-009**: System MUST enforce a minimum difficulty level so that proof-of-work is never trivially bypassed.
- **FR-010**: System MUST reject blocks with timestamps more than 120 seconds into the future relative to the node's local clock (configurable threshold, default 120s).
- **FR-011**: System MUST preserve backward compatibility with the existing block serialization format by extending rather than replacing the `Block` structure.
- **FR-012**: System MUST report consensus validation failures with descriptive error information (e.g., "invalid proof-of-work", "difficulty mismatch", "future timestamp").
- **FR-013**: System MUST enforce a configurable mining timeout (default 30 seconds) and return a descriptive error to the caller if proof-of-work computation does not complete within the limit.
- **FR-014**: System MUST enforce a configurable maximum chain reorganization depth (default 100 blocks) and reject any competing chain that would require replacing more than that many blocks.

### Key Entities

- **Block (extended)**: The existing Block structure augmented with consensus fields — nonce (proof-of-work solution), difficulty (target at time of mining), and adjusted hash computation incorporating these fields.
- **Difficulty**: An unsigned integer representing the number of leading zero bits required in the block hash. Checked via bit-shift on the SHA-256 output. Provides coarser granularity than a full 256-bit target but is simpler to implement and sufficient for expected difficulty ranges. Adjusts periodically.
- **Consensus Rules**: The set of validation rules applied to each block — proof-of-work validity, timestamp bounds, difficulty correctness, and chain linkage. Encapsulated as a validatable policy.
- **Chain State**: The canonical chain plus metadata needed for consensus decisions — current difficulty, block count since last adjustment, timestamps of recent blocks.

## Success Criteria

### Measurable Outcomes

- **SC-001**: All nodes in a personal network converge on the same canonical chain within one block interval after a fork resolves.
- **SC-002**: No invalid block (failing proof-of-work or linkage checks) is ever appended to the canonical chain.
- **SC-003**: Block production rate stays within 2x of the target block interval over any 100-block window, given stable network hashrate.
- **SC-004**: A node presented with two competing valid chains of different lengths consistently selects the longer chain.
- **SC-005**: Difficulty adjustment keeps block times within a configurable acceptable range without oscillating more than one adjustment step per period.
- **SC-006**: Existing RPC clients calling `addBlock` continue to function without modification, receiving a mined block in the response.
- **SC-007**: Mining latency on `addBlock` is imperceptible (sub-second) under typical personal-network difficulty levels.

## Assumptions

- Proof-of-Work (HashCash-style) is the appropriate consensus mechanism for this project. In the personal-use context, PoW serves as tamper-evidence and consistency enforcement rather than Sybil resistance against adversarial miners.
- Networks will consist of a small number of trusted peers (typically 2–10 nodes owned by the same user). Consensus does not need to defend against 51% attacks or selfish mining strategies.
- The target block interval will be configurable but will default to a reasonable value (e.g., 10 seconds for development/testing, adjustable for production).
- The difficulty adjustment window (number of blocks between adjustments) will be configurable with a sensible default (e.g., every 10 blocks).
- SHA-256, which is already used for block hashing, will serve as the proof-of-work hash function — no additional cryptographic dependency is needed.
- The initial difficulty for a new chain is 1 (1 leading zero bit). The minimum enforceable difficulty (FR-009) is also 1.
- The nonce field will be a 64-bit unsigned integer, providing sufficient range for realistic difficulty levels.
- Chain synchronization and block propagation (specs 003 and 005) are out of scope; this spec covers only the consensus rules and local validation/mining. The chain replacement logic will be wired into the network layer by those future specs.
- The existing `isValidNewBlock` method in `IBlockchain` will be extended (not replaced) to incorporate consensus validation.
- Single-threaded mining is acceptable for the initial implementation; parallel mining optimization is out of scope.
- Given the small trusted-network context, difficulty will remain low in practice, so mining latency on `addBlock` calls should be negligible under normal operation.
