# JSON-RPC Contract: 006 — Stream Methods

**Protocol**: JSON-RPC 2.0 over TLS  
**Port**: 12345 (default, configurable via `config.json`)  
**Transport**: TCP + TLS, newline-delimited JSON  
**Base contract**: [001 JSON-RPC contract](../../001-code-constitution-audit/contracts/json-rpc.md)

## New Methods

### `publish`

Publish a stream entry to a named stream. Auto-creates the stream if it does not exist.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "publish",
  "params": {
    "stream": "assets",
    "key": "item-42",
    "data": "any opaque string data here",
    "keys": ["optional-index-key-1"]
  }
}
```

| Param | Type | Required | Description |
|-------|------|----------|-------------|
| `stream` | string | yes | Stream name (alphanumeric, hyphens, underscores; max 256 chars) |
| `key` | string | yes | User-provided lookup key (non-empty) |
| `data` | string | no | Opaque data payload (default: empty string; max 128 MB) |
| `keys` | string[] | no | Additional index keys for `getBlocksByKeys` compatibility |

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "{\"index\":5,\"timestamp\":...,\"prevHash\":\"...\",\"hash\":\"...\",\"nonce\":...,\"difficulty\":...,\"entries\":[{\"stream\":\"assets\",\"key\":\"item-42\",\"data\":\"...\"}]}"
}
```

**Errors**:

| Code | Message | Condition |
|------|---------|-----------|
| -32602 | Invalid params: stream is required | Missing or empty `stream` |
| -32602 | Invalid params: key is required | Missing or empty `key` |
| -32602 | Invalid params: stream name invalid | Stream name violates naming rules |
| -32602 | Invalid params: data exceeds 128 MB limit | Data payload too large |
| -32003 | Stream not permitted on this node | Per-node permission check failed |
| -32001 | Sync in progress | Node is syncing |
| -32000 | Mining timeout | PoW mining exceeded timeout |

---

### `createStream`

Explicitly create a named stream without publishing data.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "createStream",
  "params": {
    "name": "inventory"
  }
}
```

| Param | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | yes | Stream name to create |

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "Stream 'inventory' created"
}
```

**Errors**:

| Code | Message | Condition |
|------|---------|-----------|
| -32602 | Invalid params: name is required | Missing or empty `name` |
| -32602 | Invalid params: stream name invalid | Name violates naming rules |
| -32004 | Stream already exists | Duplicate creation attempt |

---

### `listStreams`

List all known stream names.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "listStreams"
}
```

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "[\"assets\",\"inventory\",\"logs\"]"
}
```

---

### `getStreamEntries`

Retrieve all entries for a stream, optionally filtered by key. Returns full history in chain order.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "getStreamEntries",
  "params": {
    "stream": "assets",
    "key": "item-42"
  }
}
```

| Param | Type | Required | Description |
|-------|------|----------|-------------|
| `stream` | string | yes | Stream name to query |
| `key` | string | no | Filter by key (omit to get all entries in stream) |

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "[{\"block_index\":5,\"stream\":\"assets\",\"key\":\"item-42\",\"data\":\"...\"},{\"block_index\":12,\"stream\":\"assets\",\"key\":\"item-42\",\"data\":\"...\"}]"
}
```

**Errors**:

| Code | Message | Condition |
|------|---------|-----------|
| -32602 | Invalid params: stream is required | Missing or empty `stream` |

---

### `getStreamEntry`

Retrieve only the latest (most recent) entry for a stream + key combination.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "getStreamEntry",
  "params": {
    "stream": "assets",
    "key": "item-42"
  }
}
```

| Param | Type | Required | Description |
|-------|------|----------|-------------|
| `stream` | string | yes | Stream name to query |
| `key` | string | yes | Key to look up |

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "{\"block_index\":12,\"stream\":\"assets\",\"key\":\"item-42\",\"data\":\"...\"}"
}
```

**Errors**:

| Code | Message | Condition |
|------|---------|-----------|
| -32602 | Invalid params: stream and key are required | Missing `stream` or `key` |
| -32601 | Entry not found | No entries exist for stream + key |

## Removed Methods

### `addBlock`

The legacy `addBlock` method is removed. Use `publish` instead. Since there are no existing blockchains or clients, no backward compatibility is needed.

### `getBlockByIndex` / `getBlocksByKeys` (extended response)

Block JSON responses now include the `entries` array:

```json
{
  "index": 5,
  "timestamp": 1712793600,
  "prevHash": "abc...",
  "hash": "def...",
  "nonce": 42,
  "difficulty": 1,
  "entries": [
    {"stream": "assets", "key": "item-42", "data": "..."}
  ]
}
```
