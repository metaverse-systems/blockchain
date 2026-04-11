# JSON-RPC Contract: RPC API Expansion

**Feature**: 010-rpc-api-expansion  
**Protocol**: JSON-RPC 2.0 over TLS (newline-delimited)

## New Methods

### getNodeStatus

Returns a comprehensive snapshot of node health and state.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "getNodeStatus",
  "params": {},
  "id": "1"
}
```

**Success Response**:
```json
{
  "jsonrpc": "2.0",
  "result": {
    "chainLength": 150,
    "chunkCount": 2,
    "syncState": "idle",
    "currentDifficulty": 4,
    "inboundPeers": 2,
    "outboundPeers": 3,
    "nodeUuid": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
  },
  "id": "1"
}
```

**Error Responses**: None specific — this method has no required parameters and cannot fail under normal conditions. If `peer_manager` is null, peer counts default to 0.

---

### getBlockRange

Returns a contiguous range of blocks in ascending index order.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "getBlockRange",
  "params": {
    "startIndex": 10,
    "endIndex": 20,
    "headersOnly": false
  },
  "id": "2"
}
```

`headersOnly` is optional (default: `false`).

**Success Response** (headersOnly = false):
```json
{
  "jsonrpc": "2.0",
  "result": [
    {
      "index": 10,
      "timestamp": 1712880000,
      "prevHash": "abc123...",
      "merkleRoot": "def456...",
      "hash": "789abc...",
      "nonce": 42,
      "difficulty": 4,
      "entries": [
        {"stream": "mystream", "key": "mykey", "data": "mydata"}
      ]
    },
    ...
  ],
  "id": "2"
}
```

**Success Response** (headersOnly = true):
```json
{
  "jsonrpc": "2.0",
  "result": [
    {
      "index": 10,
      "timestamp": 1712880000,
      "prevHash": "abc123...",
      "merkleRoot": "def456...",
      "hash": "789abc...",
      "nonce": 42,
      "difficulty": 4
    },
    ...
  ],
  "id": "2"
}
```

**Error Responses**:

| Condition | Code | Message |
|-----------|------|---------|
| Missing `startIndex` or `endIndex` | -32602 | "Invalid params" |
| `startIndex` > `endIndex` | -32602 | "Invalid range: startIndex exceeds endIndex" |
| Range exceeds 1000 blocks | -32602 | "Range too large: maximum 1000 blocks per request" |
| `startIndex` >= chain length | -32001 | "Start index out of range" |

**Clamping behavior**: If `endIndex` >= chain length, it is silently clamped to `chainLength - 1`. No error is returned for this case.

---

### getChainLength

Returns the total number of blocks in the chain.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "getChainLength",
  "params": {},
  "id": "3"
}
```

**Success Response**:
```json
{
  "jsonrpc": "2.0",
  "result": 150,
  "id": "3"
}
```

**Error Responses**: None — this method always succeeds.

---

### getChunkCount

Returns the number of chunk files.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "method": "getChunkCount",
  "params": {},
  "id": "4"
}
```

**Success Response**:
```json
{
  "jsonrpc": "2.0",
  "result": 2,
  "id": "4"
}
```

**Error Responses**: None — this method always succeeds.

---

## Error Code Reference (existing + new usage)

| Code | Meaning | Used by (new methods) |
|------|---------|----------------------|
| -32602 | Invalid params | getBlockRange (missing params, start > end, range too large) |
| -32001 | Not found / out of range | getBlockRange (start index beyond chain) |

No new error codes are introduced. All codes reuse the existing allocation.
