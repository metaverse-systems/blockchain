# Feature Specification: Merkle Tree & Block Header Optimization

**Feature Branch**: `008-merkle-block-headers`  
**Created**: 2026-04-11  
**Status**: Draft  
**Input**: User description: "Implement 008 — Merkle Tree & Block Header Optimization (from Roadmap)"

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Efficient Block Verification Without Full Data (Priority: P1)

As a node operator, I want each block to contain a Merkle root computed from its entries so that I can verify that a specific transaction (stream entry) is included in a block without downloading or processing all entries in that block.

**Why this priority**: This is the foundational capability of the entire feature. Without a per-block Merkle root, no proof-of-inclusion or header-only workflows are possible. Every other story depends on this.

**Independent Test**: Can be fully tested by mining a block with multiple entries, computing its Merkle root, and confirming the root changes when any entry is added, removed, or modified. Delivers the core integrity guarantee.

**Acceptance Scenarios**:

1. **Given** a block with 5 stream entries, **When** the block is mined, **Then** the block stores a Merkle root derived from all 5 entries and the block hash incorporates the Merkle root instead of the raw entry data.
2. **Given** a block with exactly 1 entry, **When** the Merkle root is computed, **Then** the root equals the hash of that single entry.
3. **Given** a block with 0 entries (empty block), **When** the Merkle root is computed, **Then** the root is a well-defined empty-tree value (e.g., hash of empty string) and the block remains valid.
4. **Given** two blocks with the same entries in different order, **When** their Merkle roots are computed, **Then** the roots differ (entry order is significant).

---

### User Story 2 - Proof-of-Inclusion for Stream Entries (Priority: P2)

As a client querying the blockchain, I want to request a Merkle proof that a specific stream entry exists in a given block so that I can verify inclusion without trusting the node to return accurate data. Proofs are requested and verified via JSON-RPC endpoints.

**Why this priority**: This delivers the primary user-facing value of Merkle trees — trustless verification. Without proof generation and verification, the Merkle root is an internal optimization only.

**Independent Test**: Can be fully tested by generating a proof for a known entry in a block, then verifying the proof against the block's stored Merkle root. Also testable by attempting verification with a tampered entry (must fail).

**Acceptance Scenarios**:

1. **Given** a block containing entry E at position 3 of 8 entries, **When** a Merkle proof is requested for E, **Then** the system returns a proof consisting of sibling hashes along the path from E's leaf to the root.
2. **Given** a valid Merkle proof and the correct entry data, **When** the proof is verified against the block's Merkle root, **Then** verification succeeds.
3. **Given** a valid Merkle proof but tampered entry data, **When** the proof is verified, **Then** verification fails.
4. **Given** a block that does not contain entry E, **When** a proof is requested for E, **Then** the system clearly reports that the entry is not found in the block.

---

### User Story 3 - Lightweight Block Headers for P2P Propagation (Priority: P3)

As a node operator, I want blocks to have a distinct header (containing index, timestamp, previous hash, Merkle root, nonce, and difficulty) separate from the full entry payload so that peers can exchange and validate block headers without transmitting full entry data.

**Why this priority**: Header-only propagation reduces bandwidth for initial block announcements and enables future light-client support. It builds on the Merkle root from P1 but is independently useful for network efficiency.

**Independent Test**: Can be fully tested by constructing a block, extracting its header, serializing/deserializing the header independently, and confirming the header hash matches the full block's hash.

**Acceptance Scenarios**:

1. **Given** a newly mined block, **When** a peer receives the block header, **Then** the peer can validate proof-of-work and hash-chain continuity using only the header fields (without entry data).
2. **Given** a block header received from a peer, **When** the receiving node already has the entries (e.g., from a previous transaction relay), **Then** it can reconstruct and verify the full block by combining header and entries.
3. **Given** a serialized block header, **When** its size is measured, **Then** it is fixed-size and independent of the number of entries in the block.

---

### Edge Cases

- What happens when a block contains entries that individually exceed typical sizes (e.g., entries near the 128 MB limit)? The Merkle tree must handle arbitrarily large leaf data by hashing each entry before tree construction.
- What happens when a block has an odd number of entries? The Merkle tree must define a consistent strategy for handling non-power-of-two leaf counts (e.g., duplicating the last leaf or carrying it up).
- What happens when two entries in the same block are identical? The tree must still produce a correct root and proofs — duplicate entries at different positions must have distinguishable proofs.
- How does the system handle a corrupted Merkle proof (truncated or with extra elements)? Verification must reject malformed proofs without crashing.

## Clarifications

### Session 2026-04-11

- Q: Should the Merkle tree use domain-separated hashing to prevent second-preimage attacks? → A: Yes — prefix leaf hashes with 0x00, internal node hashes with 0x01 (per RFC 6962).
- Q: What is the expected typical number of stream entries per block? → A: 1–100 entries, yielding Merkle proofs ≤7 hashes deep.
- Q: How should Merkle proofs be exposed to clients? → A: Via JSON-RPC endpoints (e.g., `getInclusionProof`, `verifyInclusionProof`).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Each block MUST store a Merkle root field computed from the SHA-256 hashes of its stream entries, ordered by their position in the entries array.
- **FR-002**: The block hash computation MUST incorporate the Merkle root in place of the raw serialized entry data, so that the hash commits to all entries via the root.
- **FR-003**: The system MUST compute per-block Merkle trees using SHA-256 as the hash function, consistent with the existing block hash algorithm.
- **FR-004**: For blocks with zero entries, the Merkle root MUST be set to a well-defined constant (the SHA-256 hash of the empty string).
- **FR-005**: For blocks with an odd number of entries at any tree level, the system MUST duplicate the last node at that level to form a complete binary tree.
- **FR-006**: The system MUST provide a way to generate a Merkle inclusion proof for any entry in a block, given the block index and the entry's position.
- **FR-007**: The system MUST provide a way to verify a Merkle inclusion proof against a block's stored Merkle root.
- **FR-008**: Blocks MUST have a logically distinct header consisting of: index, timestamp, previous hash, Merkle root, nonce, difficulty, and the computed block hash.
- **FR-009**: The block header MUST be independently serializable to JSON and to Boost binary archive, separate from the full block payload.
- **FR-010**: The Merkle tree leaf hash for each entry MUST be computed by prefixing the entry's serialized form with a 0x00 byte and then hashing (SHA-256), ensuring domain separation between leaves and internal nodes.
- **FR-011**: Internal Merkle tree node hashes MUST be computed by prefixing the concatenation of the two child hashes with a 0x01 byte and then hashing (SHA-256), preventing second-preimage attacks per RFC 6962.
- **FR-012**: Merkle proof generation and verification MUST be accessible via JSON-RPC endpoints (e.g., `getInclusionProof` and `verifyInclusionProof`), consistent with the existing RPC interface.
- **FR-013**: Block header retrieval MUST be accessible via a JSON-RPC endpoint (e.g., `getBlockHeader`) that returns only header fields without entry data.

### Key Entities

- **Block Header**: The subset of a block's metadata sufficient for chain validation without entry data — includes index, timestamp, previous hash, Merkle root, nonce, difficulty, and block hash.
- **Merkle Tree**: A binary hash tree constructed from the SHA-256 hashes of a block's stream entries. Used to produce the Merkle root stored in the block header.
- **Merkle Proof**: An ordered list of sibling hashes along the path from a specific entry's leaf to the Merkle root, plus the entry's position, sufficient to independently verify inclusion.
- **Stream Entry (existing)**: A named-stream key/value record. Each entry becomes one leaf in the block's Merkle tree.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Any stream entry's inclusion in a block can be verified using only the block header and a Merkle proof, without access to other entries in the block.
- **SC-002**: Block header data is fixed-size regardless of the number of entries in the block, enabling predictable bandwidth for header-only exchanges.
- **SC-003**: Merkle proof generation and verification complete in time proportional to the logarithm of the entry count (not linear in entry count).

## Assumptions

- The existing SHA-256 implementation (OpenSSL EVP) will be reused for all Merkle tree hash computations; no new cryptographic dependencies are needed.
- The Merkle tree is per-block (over stream entries within a single block), not per-chunk. A per-chunk Merkle tree is out of scope for this spec and may be considered in future work.
- Entry ordering within a block is stable and deterministic — the Merkle tree is built over entries in the order they appear in the block's entries array.
- Typical blocks contain 1–100 stream entries, resulting in Merkle tree depths of ≤7 levels. No intermediate tree caching is required at this scale.
- All blocks will include the Merkle root field; there are no existing chains to maintain backward compatibility with.
- The odd-leaf duplication strategy (duplicate the last node) is a standard approach used by Bitcoin and other blockchains; no custom handling is needed.
- Light-client protocol (full remote verification workflow) is out of scope — this spec provides the data structures and APIs that a future light-client protocol (spec 017) would build upon.
- P2P message format changes for header-only propagation are out of scope; this spec ensures headers are structurally separable. Wire protocol changes would be a follow-on.
