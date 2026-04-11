# Research: Merkle Tree & Block Header Optimization

**Feature**: 008-merkle-block-headers  
**Date**: 2026-04-11

## R1: Merkle Tree Construction with Domain Separation (RFC 6962)

**Decision**: Use RFC 6962 domain-separated hashing with 0x00 leaf prefix and 0x01 internal prefix.

**Rationale**: RFC 6962 (Certificate Transparency) defines a widely-adopted standard for Merkle tree construction that prevents second-preimage attacks. The scheme is simple:
- Leaf hash: `SHA-256(0x00 || leaf_data)`
- Internal hash: `SHA-256(0x01 || left_child_hash || right_child_hash)`

This ensures no internal node can be confused with a leaf and vice versa, even if an attacker controls the leaf data. The overhead is exactly 1 byte per hash computation — negligible.

**Implementation approach**: Create a standalone `MerkleTree` module with:
- `computeMerkleRoot(vector<string> leafHashes)` — builds tree bottom-up, returns hex root
- `generateProof(vector<string> leafHashes, size_t index)` — returns vector of `{hash, isLeft}` pairs
- `verifyProof(string root, string leafHash, vector<ProofElement> proof)` — recomputes root from proof and compares

For odd node counts at any level: duplicate the last node (Bitcoin convention). This is simple, well-understood, and matches the spec requirement (FR-005).

**Alternatives considered**:
- No prefix (plain concatenation) — Rejected: vulnerable to second-preimage attacks
- HMAC-based approach — Rejected: unnecessary complexity; simple prefix is sufficient and standard
- Zero-padded even tree (pad to power of 2) — Rejected: wastes space; odd-node duplication is simpler

## R2: Boost.Serialization — Adding `merkleRoot` Field to Block

**Decision**: Add `merkleRoot` as a new `std::string` field to `Block`, serialized after existing fields in the `serialize()` template.

**Rationale**: Since there are no existing chains to worry about (confirmed in clarification), we can modify the serialization format freely. The field is added at the end of the `serialize()` template:

```
ar & index;
ar & timestamp;
ar & entries;
ar & prevHash;
ar & hash;
ar & nonce;
ar & difficulty;
ar & merkleRoot;  // NEW
```

No versioning or optional deserialization needed — all blocks will have this field from the start.

**Alternatives considered**:
- Boost.Serialization versioning (`BOOST_CLASS_VERSION`) — Rejected: unnecessary since there are no pre-existing serialized blocks without the field
- Storing Merkle root inside the hash field — Rejected: the root is a separate concept from the block hash; conflating them would break the clear separation

## R3: Entry Leaf Hash Determinism

**Decision**: Hash each entry by Boost.Serialization binary-archiving the `StreamEntry` struct and prefixing with 0x00 before SHA-256.

**Rationale**: The codebase already uses Boost.Serialization binary archives to serialize entries into the block hash (see `Block::calculateHash()`). Reusing this exact serialization for leaf hashes ensures:
1. Determinism — same entry always produces the same binary output
2. Consistency — the same serialization method used everywhere
3. All three fields (stream, key, data) are included in the hash

The leaf hash computation:
```
serialize StreamEntry → binary bytes
SHA-256(0x00 || binary_bytes) → leaf_hash (hex string)
```

**Alternatives considered**:
- String concatenation (`stream + key + data`) — Rejected: ambiguous boundaries (is "ab" + "cd" the same as "a" + "bcd"?)
- JSON serialization — Rejected: non-deterministic key ordering across platforms; significantly slower
- Custom binary format — Rejected: Boost.Serialization is already the standard in this codebase

## R4: Block Hash Computation Change

**Decision**: Replace raw serialized entry data in `calculateHash()` with the Merkle root string.

**Rationale**: Currently `calculateHash()` serializes the entire entries vector via Boost binary archive and includes those bytes in the hash input. Post-change:

Before: `SHA-256(index || timestamp || boost_serialize(entries) || prevHash || nonce || difficulty)`
After:  `SHA-256(index || timestamp || merkleRoot || prevHash || nonce || difficulty)`

This makes the block hash commit to the entries indirectly via the Merkle root, while keeping the hash computation fast and fixed-size (the Merkle root is always a 64-char hex string, regardless of entry count).

The `merkleRoot` must be computed *before* `calculateHash()` is called — typically in the Block constructor or a `computeMerkleRoot()` setter.

**Alternatives considered**:
- Include both Merkle root and raw entries in hash — Rejected: redundant; the root already commits to all entries
- Keep raw entries and add root as an extra field — Rejected: misses the performance benefit of not serializing all entries during hash computation

## R5: Block Header Separation

**Decision**: Introduce a `BlockHeader` concept as a method that extracts header fields, not a separate persisted struct.

**Rationale**: The Block struct already contains all header fields (index, timestamp, prevHash, merkleRoot, nonce, difficulty, hash). Rather than introducing a separate `BlockHeader` struct that duplicates data, we add:
1. A `toHeaderJson()` method on Block that returns only header fields
2. The block hash is computed from header fields only (since `calculateHash()` will use merkleRoot instead of raw entries)

This keeps the data model simple while satisfying FR-008 (logically distinct header) and FR-009 (independently serializable). A separate serialization path for the header allows P2P header-only exchanges.

**Alternatives considered**:
- Full separate `BlockHeader` class — Rejected for this spec: adds complexity. The header is a view of the block, not a separate entity. A future light-client spec (017) may introduce a standalone header struct if needed.
- Composition (Block contains BlockHeader + entries) — Rejected: requires restructuring every serialization and constructor call in the codebase for marginal benefit at this stage.
