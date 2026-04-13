# Module Interface Contract: MerkleProofService

**Module**: `MerkleProofService`  
**Header**: `src/MerkleProofService.hpp`  
**Implementation**: `src/MerkleProofService.cpp`

## Construction

```
MerkleProofService() = default;
```

Stateless — no owned fields. All data passed per call.

## Public Methods

```
nlohmann::json getInclusionProof(const Block& block, size_t entryIndex)
```
Generates a Merkle inclusion proof for a specific entry within a block.

**Parameters**:
- `block`: The block containing the entry
- `entryIndex`: Index of the entry within `block.entries`

**Returns**: JSON object with fields:
- `blockIndex`: Block index
- `entryIndex`: Entry index within block
- `merkleRoot`: Block's Merkle root (hex)
- `leafHash`: SHA-256 of serialized entry (hex)
- `proof`: Array of `{hash, isLeft}` elements tracing from leaf to root

**Errors**: Throws `std::runtime_error` if `entryIndex >= block.entries.size()`.

---

```
nlohmann::json verifyInclusionProof(const Block& block,
                                     const std::string& leafHash,
                                     const nlohmann::json& proofArray)
```
Verifies a Merkle inclusion proof against a block's stored Merkle root.

**Parameters**:
- `block`: The block to verify against
- `leafHash`: Claimed leaf hash (hex)
- `proofArray`: JSON array of `{hash, isLeft}` proof elements

**Returns**: JSON object with fields:
- `valid`: Boolean — whether proof verifies
- `expectedRoot`: Block's stored Merkle root
- `computedRoot`: Root recomputed from leaf + proof path

## Dependencies

- `MerkleTree::computeLeafHash()` — hashes serialized entries
- `MerkleTree::generateProof()` — builds proof path
- `MerkleTree::verifyProof()` — recomputes and checks root
- `boost::archive::binary_oarchive` — serializes StreamEntry for leaf hashing
