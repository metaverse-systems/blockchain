# JSON-RPC Contracts: Merkle Tree & Block Header Optimization

**Feature**: 008-merkle-block-headers  
**Date**: 2026-04-11  
**Protocol**: JSON-RPC 2.0 over TLS

## New Endpoints

### getInclusionProof

Generate a Merkle inclusion proof for a specific entry in a block.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "getInclusionProof",
  "params": {
    "blockIndex": 42,
    "entryIndex": 3
  }
}
```

| Param | Type | Required | Description |
|-------|------|----------|-------------|
| blockIndex | integer | Yes | Index of the block containing the entry |
| entryIndex | integer | Yes | Zero-based position of the entry in the block's entries array |

**Success Response**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": {
    "blockIndex": 42,
    "entryIndex": 3,
    "merkleRoot": "a1b2c3...64-char-hex",
    "leafHash": "d4e5f6...64-char-hex",
    "proof": [
      { "hash": "aabb...64-char-hex", "isLeft": true },
      { "hash": "ccdd...64-char-hex", "isLeft": false },
      { "hash": "eeff...64-char-hex", "isLeft": true }
    ]
  }
}
```

| Result Field | Type | Description |
|--------------|------|-------------|
| blockIndex | integer | Echoed block index |
| entryIndex | integer | Echoed entry index |
| merkleRoot | string | 64-char hex Merkle root of the block |
| leafHash | string | 64-char hex hash of the target entry (with 0x00 domain prefix) |
| proof | array | Ordered sibling hashes from leaf to root |
| proof[].hash | string | 64-char hex sibling node hash |
| proof[].isLeft | boolean | Whether this sibling is on the left side of the concatenation |

**Error Responses**:

| Code | Message | When |
|------|---------|------|
| -32602 | Invalid params | Missing or non-integer blockIndex/entryIndex |
| -32001 | Block not found | blockIndex is out of range |
| -32002 | Entry not found | entryIndex is out of range for the given block |

---

### verifyInclusionProof

Verify a Merkle inclusion proof against a block's stored Merkle root.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "2",
  "method": "verifyInclusionProof",
  "params": {
    "blockIndex": 42,
    "leafHash": "d4e5f6...64-char-hex",
    "proof": [
      { "hash": "aabb...64-char-hex", "isLeft": true },
      { "hash": "ccdd...64-char-hex", "isLeft": false },
      { "hash": "eeff...64-char-hex", "isLeft": true }
    ]
  }
}
```

| Param | Type | Required | Description |
|-------|------|----------|-------------|
| blockIndex | integer | Yes | Index of the block to verify against |
| leafHash | string | Yes | 64-char hex hash of the entry to verify |
| proof | array | Yes | Ordered sibling hashes from leaf to root |
| proof[].hash | string | Yes | 64-char hex sibling node hash |
| proof[].isLeft | boolean | Yes | Left/right position of the sibling |

**Success Response**:
```json
{
  "jsonrpc": "2.0",
  "id": "2",
  "result": {
    "valid": true,
    "merkleRoot": "a1b2c3...64-char-hex"
  }
}
```

| Result Field | Type | Description |
|--------------|------|-------------|
| valid | boolean | Whether the proof verifies against the block's Merkle root |
| merkleRoot | string | 64-char hex Merkle root of the block (for client cross-check) |

**Error Responses**:

| Code | Message | When |
|------|---------|------|
| -32602 | Invalid params | Missing params, non-string leafHash, malformed proof array |
| -32001 | Block not found | blockIndex is out of range |

Note: An invalid proof (that doesn't match the root) is NOT an error — it returns `{"valid": false}`. Errors are reserved for malformed requests.

---

### getBlockHeader

Retrieve only the header fields of a block (without entry data).

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "3",
  "method": "getBlockHeader",
  "params": {
    "blockIndex": 42
  }
}
```

| Param | Type | Required | Description |
|-------|------|----------|-------------|
| blockIndex | integer | Yes | Index of the block |

**Success Response**:
```json
{
  "jsonrpc": "2.0",
  "id": "3",
  "result": {
    "index": 42,
    "timestamp": 1744329600,
    "prevHash": "0011...64-char-hex",
    "merkleRoot": "a1b2...64-char-hex",
    "nonce": 12345,
    "difficulty": 4,
    "hash": "00ab...64-char-hex"
  }
}
```

**Error Responses**:

| Code | Message | When |
|------|---------|------|
| -32602 | Invalid params | Missing or non-integer blockIndex |
| -32001 | Block not found | blockIndex is out of range |

---

## Modified Endpoints

### getBlockByIndex (existing — extended response)

The existing `getBlockByIndex` response now includes the `merkleRoot` field in addition to all current fields.

**Additional field in result**:
```json
{
  "merkleRoot": "a1b2c3...64-char-hex"
}
```

This field appears alongside existing fields (`index`, `timestamp`, `prevHash`, `hash`, `nonce`, `difficulty`, `entries`). No changes to request format.

### publish (existing — extended response)

The existing `publish` response (which returns the full block JSON) now includes the `merkleRoot` field. No changes to request format.
