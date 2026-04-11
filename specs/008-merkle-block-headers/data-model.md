# Data Model: Merkle Tree & Block Header Optimization

**Feature**: 008-merkle-block-headers  
**Date**: 2026-04-11

## Modified Entities

### Block (modified)

Existing entity with one new field added.

| Field | Type | New? | Description |
|-------|------|------|-------------|
| index | size_t | No | Block sequential position in chain |
| timestamp | uint64_t | No | Unix timestamp of creation |
| entries | vector\<StreamEntry\> | No | Stream key-value entries (transaction data) |
| prevHash | string | No | SHA-256 hex of previous block |
| hash | string | No | SHA-256 hex of this block's header fields |
| nonce | uint64_t | No | Proof-of-work counter |
| difficulty | uint32_t | No | Target leading zero bits |
| **merkleRoot** | **string** | **Yes** | **SHA-256 hex Merkle root of entries** |

**Relationships**:
- Contains 0..N `StreamEntry` records (entries array)
- References previous Block via `prevHash`
- `merkleRoot` is derived from `entries` via Merkle tree construction

**Validation rules**:
- `merkleRoot` must equal the result of computing the Merkle tree over `entries`
- `hash` must equal `SHA-256(index || timestamp || merkleRoot || prevHash || nonce || difficulty)`

**State transitions**:
1. Entries set → `merkleRoot` computed from entries via Merkle tree
2. `merkleRoot` computed → `hash` computed from header fields (including merkleRoot)
3. Block mined (nonce incremented until hash meets difficulty target)

### Block Header (logical view)

Not a separate persisted entity. A logical subset of Block used for header-only operations.

| Field | Type | Description |
|-------|------|-------------|
| index | size_t | Block sequential position |
| timestamp | uint64_t | Unix timestamp |
| prevHash | string | SHA-256 hex of previous block |
| merkleRoot | string | SHA-256 hex Merkle root of entries |
| nonce | uint64_t | Proof-of-work counter |
| difficulty | uint32_t | Target leading zero bits |
| hash | string | SHA-256 hex of this header |

**Serialization**: Independently serializable via Boost binary archive or JSON. Fixed size (~200 bytes binary, regardless of entry count).

## New Entities

### MerkleProof

Represents a proof-of-inclusion for a single entry in a block.

| Field | Type | Description |
|-------|------|-------------|
| blockIndex | size_t | Index of the block containing the entry |
| entryIndex | size_t | Position of the entry within the block's entries array |
| leafHash | string | SHA-256 hex hash of the target entry (with 0x00 prefix) |
| siblings | vector\<MerkleProofElement\> | Ordered path from leaf to root |

**Relationships**:
- References a Block via `blockIndex`
- References a StreamEntry via `entryIndex`
- Verified against the Block's `merkleRoot`

### MerkleProofElement

One step in a Merkle proof path.

| Field | Type | Description |
|-------|------|-------------|
| hash | string | SHA-256 hex of the sibling node |
| isLeft | bool | Whether this sibling is on the left side of the pair |

## Merkle Tree Construction Rules

1. **Leaf hashing**: Each `StreamEntry` is serialized via Boost binary archive, then hashed as `SHA-256(0x00 || serialized_bytes)`
2. **Internal hashing**: Each internal node is `SHA-256(0x01 || left_child_hash || right_child_hash)` where child hashes are raw 32-byte SHA-256 digests
3. **Odd node count**: When a tree level has an odd number of nodes, the last node is duplicated
4. **Empty tree**: When entries is empty, the Merkle root is `SHA-256(empty string)` — a well-defined constant
5. **Single entry**: The Merkle root equals the leaf hash of that entry
6. **Entry ordering**: Entries are processed in array order (position 0 first)

## Entity Relationship Summary

```
Block
├── merkleRoot (derived from entries via Merkle tree)
├── hash (derived from header fields including merkleRoot)
├── entries: StreamEntry[0..N]
│   └── Each entry → one leaf in Merkle tree
└── prevHash → previous Block.hash

MerkleProof
├── blockIndex → Block.index
├── entryIndex → Block.entries[entryIndex]
├── leafHash (hash of target entry)
└── siblings: MerkleProofElement[]
    └── path from leaf to Block.merkleRoot
```
