# JSON-RPC Test Contract

**Date**: 2026-04-11
**Feature**: 012-integration-test-suite

This contract documents the JSON-RPC methods that the integration tests validate. Each method lists its request format, expected success response, and expected error conditions.

**Protocol**: Newline-delimited JSON-RPC 2.0 over TLS (TCP).

## Request Format

All requests follow JSON-RPC 2.0:

```json
{"jsonrpc": "2.0", "id": "<string>", "method": "<method>", "params": {<params>}}
```

Terminated by `\n`. The `id` field is required.

## Success Response Format

```json
{"jsonrpc": "2.0", "id": "<string>", "result": <value>}
```

## Error Response Format

```json
{"jsonrpc": "2.0", "id": "<string>", "error": {"code": <int>, "message": "<string>"}}
```

---

## Methods

### getChainLength

**Params**: none
**Result**: `<integer>` — number of blocks (including genesis)
**Errors**: none expected

### getChunkCount

**Params**: none
**Result**: `<integer>` — number of chunk files
**Errors**: none expected

### getNodeStatus

**Params**: none
**Result**: object with fields:
- `chainLength`: integer
- `chunkCount`: integer
- `syncState`: `"idle"` | `"syncing"`
- `currentDifficulty`: integer
- `inboundPeers`: integer
- `outboundPeers`: integer
- `nodeUuid`: string

**Errors**: none expected

### getBlockRange

**Params**:
- `start`: integer (required) — starting block index
- `end`: integer (required) — ending block index (inclusive)
- `headersOnly`: boolean (optional, default false)

**Result**: array of block objects (or header objects if headersOnly=true)
**Errors**:
- `-32602` if `start > end` or range exceeds 1000 blocks
- `-32001` if block index out of range

### getBlockHeader

**Params**:
- `blockIndex`: integer (required)

**Result**: object with fields: `index`, `timestamp`, `prevHash`, `merkleRoot`, `nonce`, `difficulty`, `hash`
**Errors**:
- `-32001` if block index out of range

### getInclusionProof

**Params**:
- `blockIndex`: integer (required)
- `entryIndex`: integer (required)

**Result**: object with fields: `blockIndex`, `entryIndex`, `merkleRoot`, `leafHash`, `proof` (array of hex strings)
**Errors**:
- `-32001` if block index out of range
- `-32002` if entry index out of range

### verifyInclusionProof

**Params**:
- `blockIndex`: integer (required)
- `leafHash`: string (required, 64-char hex)
- `proof`: array of strings (required)

**Result**: object with fields: `valid` (boolean), `merkleRoot` (string)
**Errors**:
- `-32001` if block index out of range

### publish

**Params**:
- `stream`: string (required)
- `key`: string (required)
- `data`: string (required)
- `keys`: array of strings (required)

**Result**: block object (the newly mined block)
**Errors**:
- `-32003` if stream not in allowed_streams
- `-32603` if sync in progress

### getStreamEntries

**Params**:
- `stream`: string (required)
- `key`: string (optional)
- `mode`: `"history"` | `"latest"` (optional, default `"history"`)

**Result**: array of entry objects
**Errors**: none (empty array if no entries)

### requestSync

**Params**: none
**Result**: `"sync started"` or similar confirmation
**Errors**:
- `-32603` if no peer client available

### addBlock (legacy)

**Params**:
- `data`: string (required)

**Result**: block object
**Errors**: none expected for valid data

---

## Error Codes Reference

| Code | Meaning |
|------|---------|
| -32600 | Invalid JSON-RPC request |
| -32601 | Method not found |
| -32602 | Invalid params |
| -32603 | Internal error / not available |
| -32001 | Block not found |
| -32002 | Entry not found |
| -32003 | Stream not permitted / connection limit |
| -32004 | Already exists |
| -32005 | Peer not found |
| -32006 | Peer not banned |

---

## Test Coverage Matrix

Each method must be tested for at least one positive case and applicable negative cases:

| Method | Positive Test | Negative/Error Test |
|--------|--------------|-------------------|
| getChainLength | Call and verify count | (no error path) |
| getChunkCount | Call and verify count | (no error path) |
| getNodeStatus | Verify all 7 fields present | (no error path) |
| getBlockRange | Valid range returns blocks | Invalid range returns -32602 |
| getBlockHeader | Valid index returns header | Invalid index returns -32001 |
| getInclusionProof | Valid block+entry returns proof | Invalid index returns -32001/-32002 |
| verifyInclusionProof | Known-good proof returns valid=true | Tampered hash returns valid=false |
| publish | Publish entry, chain grows | Disallowed stream returns -32003 |
| getStreamEntries | Fetch published entries | Empty stream returns empty array |
| requestSync | Acknowledge with no peer | Returns -32603 (no peer) |
| (unknown method) | — | Returns -32601 |
| (malformed JSON) | — | Returns -32600 |
