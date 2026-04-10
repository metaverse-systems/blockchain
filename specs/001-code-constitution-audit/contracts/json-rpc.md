# JSON-RPC Contract

**Protocol**: JSON-RPC 2.0 over TLS (server-only verification)
**Port**: 12345 (default)
**Transport**: TCP + TLS, newline-delimited JSON

## Authentication

- Server presents TLS certificate
- Client certificate is **not** required (server-only TLS)
- Connection requires valid TLS handshake

## Methods

### `addBlock`

Add a new block to the blockchain.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "addBlock",
  "params": {
    "data": "<string>",
    "keys": ["<string>", ...]
  }
}
```

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "{\"index\":1,\"timestamp\":...,\"data\":\"...\",\"prevHash\":\"...\",\"hash\":\"...\"}"
}
```

**Note**: `result` is a JSON-encoded string of the Block object.

---

### `getBlockByIndex`

Retrieve a block by its sequential index.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "getBlockByIndex",
  "params": {
    "index": <number>
  }
}
```

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "{\"index\":0,\"timestamp\":...,\"data\":\"...\",\"prevHash\":\"...\",\"hash\":\"...\"}"
}
```

**Error (invalid params)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32602,
    "message": "Invalid parameters"
  }
}
```

---

### `getBlocksByKeys`

Retrieve all blocks matching the given keys.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "getBlocksByKeys",
  "params": {
    "keys": ["<string>", ...]
  }
}
```

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "[{\"index\":...,\"timestamp\":...,\"data\":\"...\",\"prevHash\":\"...\",\"hash\":\"...\"},...]"
}
```

---

## Error Codes

| Code | Message | Condition |
|------|---------|-----------|
| -32600 | Invalid JSON-RPC message | Missing or wrong `jsonrpc` field |
| -32600 | JSON-RPC requests must include an 'id' | Missing `id` field |
| -32601 | Invalid method: {method} | Unknown method name |
| -32602 | Invalid parameters | Missing or malformed `params` |

## Changes in This Audit

- **FR-004b**: Server-only TLS — no client certificate required
- **FR-008**: Handshake failures logged with structured output
- **FR-009**: All async operations timeout after 30 seconds (configurable)
- **FR-013**: `addBlock` calls are serialized via strand for thread safety
