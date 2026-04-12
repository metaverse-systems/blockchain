# RPC API Reference

The blockchain node exposes a JSON-RPC 2.0 API over raw TLS (not HTTP). Send a newline-terminated JSON object over a TLS connection to the node's RPC port (default: 12345).

> **Pre-1.0 stability notice**: This project is pre-1.0 and actively evolving. The JSON-RPC API may change without backward compatibility guarantees.

## Table of Contents

- [Request Format](#request-format)
- [Streams](#streams) — `publish`, `createStream`, `listStreams`, `getStreamEntries`, `getStreamEntry`
- [Blocks](#blocks) — `getBlockByIndex`, `getBlocksByKeys`, `getBlockRange`
- [Peers](#peers) — `addPeer`, `removePeer`, `listPeers`, `banPeer`, `unbanPeer`
- [Node](#node) — `getNodeStatus`, `getChainLength`, `getChunkCount`
- [Merkle](#merkle) — `getInclusionProof`, `verifyInclusionProof`, `getBlockHeader`
- [Sync](#sync) — `requestSync`
- [Error Codes](#error-codes)

## Request Format

All requests use the JSON-RPC 2.0 envelope:

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "<method_name>",
  "params": { ... }
}
```

All examples in this document use `openssl s_client` to send JSON-RPC over a raw TLS connection. Replace `ca.pem` with the path to your CA certificate and `localhost:12345` with your node's address.

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"<method_name>","params":{...}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

---

## Streams

### `publish`

Publish a stream entry and mine a new block containing it.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `stream` | string | yes | Target stream name |
| `key` | string | yes | Entry key |
| `data` | string | no | Entry data (max 128 MB) |
| `keys` | array of strings | no | Additional lookup keys for the block |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "publish",
  "params": {
    "stream": "mystream",
    "key": "greeting",
    "data": "hello world"
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "index": 1,
    "timestamp": 1712937600,
    "prevHash": "0000...",
    "hash": "00ab...",
    "nonce": 42,
    "difficulty": 1,
    "merkleRoot": "a1b2...",
    "entries": [
      {
        "stream": "mystream",
        "key": "greeting",
        "data": "hello world"
      }
    ]
  }
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"publish","params":{"stream":"mystream","key":"greeting","data":"hello world"}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid or missing parameters
- `-32003` — stream not permitted by `allowed_streams` configuration
- `-32000` — mining timeout exceeded
- `-32001` — node is syncing (publish blocked during sync)

---

### `createStream`

Explicitly create a named stream. Streams are also created implicitly on first `publish`.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `name` | string | yes | Stream name to create |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "createStream",
  "params": {
    "name": "mystream"
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": "Stream 'mystream' created"
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"createStream","params":{"name":"mystream"}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid or missing parameters
- `-32004` — stream already exists

---

### `listStreams`

List all known stream names.

**Parameters:** None.

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "listStreams"
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": ["mystream", "events", "logs"]
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"listStreams"}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

---

### `getStreamEntries`

Get all entries for a stream, optionally filtered by key.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `stream` | string | yes | Stream name |
| `key` | string | no | Filter by key (omit for all entries) |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getStreamEntries",
  "params": {
    "stream": "mystream"
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": [
    {
      "block_index": 1,
      "stream": "mystream",
      "key": "greeting",
      "data": "hello world"
    }
  ]
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getStreamEntries","params":{"stream":"mystream"}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid or missing parameters

---

### `getStreamEntry`

Get the latest entry for a specific stream and key.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `stream` | string | yes | Stream name |
| `key` | string | yes | Entry key |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getStreamEntry",
  "params": {
    "stream": "mystream",
    "key": "greeting"
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "block_index": 1,
    "stream": "mystream",
    "key": "greeting",
    "data": "hello world"
  }
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getStreamEntry","params":{"stream":"mystream","key":"greeting"}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid or missing parameters
- `-32601` — entry not found

---

## Blocks

### `getBlockByIndex`

Get a single block by its chain index.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `index` | integer | yes | Block index (0-based) |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getBlockByIndex",
  "params": {
    "index": 0
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "index": 0,
    "timestamp": 1712937600,
    "prevHash": "",
    "hash": "0000...",
    "nonce": 0,
    "difficulty": 1,
    "merkleRoot": "...",
    "entries": []
  }
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getBlockByIndex","params":{"index":0}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid parameters or index out of range

---

### `getBlocksByKeys`

Get all blocks that contain entries matching any of the specified keys.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `keys` | array of strings | yes | Keys to search for |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getBlocksByKeys",
  "params": {
    "keys": ["greeting", "farewell"]
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": [
    {
      "index": 1,
      "timestamp": 1712937600,
      "prevHash": "0000...",
      "hash": "00ab...",
      "nonce": 42,
      "difficulty": 1,
      "merkleRoot": "...",
      "entries": [...]
    }
  ]
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getBlocksByKeys","params":{"keys":["greeting"]}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid or missing parameters

---

### `getBlockRange`

Get a range of blocks by index. Maximum 1000 blocks per request.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `startIndex` | integer | yes | First block index (inclusive) |
| `endIndex` | integer | yes | Last block index (inclusive) |
| `headersOnly` | bool | no | If `true`, return block headers without entries (default: `false`) |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getBlockRange",
  "params": {
    "startIndex": 0,
    "endIndex": 9
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": [
    {
      "index": 0,
      "timestamp": 1712937600,
      "prevHash": "",
      "hash": "0000...",
      "nonce": 0,
      "difficulty": 1,
      "merkleRoot": "..."
    }
  ]
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getBlockRange","params":{"startIndex":0,"endIndex":9}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid range, range exceeds 1000 blocks, or `startIndex` > `endIndex`
- `-32001` — start index out of range

---

## Peers

### `addPeer`

Connect to a remote peer by host and P2P port.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `host` | string | yes | Peer hostname or IP |
| `port` | integer | yes | Peer P2P port |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "addPeer",
  "params": {
    "host": "10.0.0.2",
    "port": 12346
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": "peer_added"
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"addPeer","params":{"host":"10.0.0.2","port":12346}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid or missing parameters
- `-32603` — peer manager unavailable
- `-32004` — peer is banned
- `-32003` — outbound connection limit reached

---

### `removePeer`

Disconnect and remove a peer.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `host` | string | yes | Peer hostname or IP |
| `port` | integer | yes | Peer P2P port |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "removePeer",
  "params": {
    "host": "10.0.0.2",
    "port": 12346
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": "peer_removed"
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"removePeer","params":{"host":"10.0.0.2","port":12346}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid or missing parameters
- `-32603` — peer manager unavailable
- `-32005` — peer not found

---

### `listPeers`

List all connected peers and active bans.

**Parameters:** None.

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "listPeers"
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "node_uuid": "abc123-...",
    "discovery_enabled": true,
    "outbound_count": 1,
    "inbound_count": 0,
    "max_outbound": 8,
    "max_inbound": 32,
    "peers": [
      {
        "host": "10.0.0.2",
        "port": 12346
      }
    ],
    "bans": []
  }
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"listPeers"}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32603` — peer manager unavailable

---

### `banPeer`

Ban a peer for a specified duration. Banned peers are immediately disconnected and cannot reconnect until the ban expires.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `host` | string | yes | Peer hostname or IP |
| `port` | integer | yes | Peer P2P port |
| `duration_seconds` | integer | no | Ban duration in seconds (default: `ban_duration_seconds` from config, typically 3600) |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "banPeer",
  "params": {
    "host": "10.0.0.2",
    "port": 12346,
    "duration_seconds": 7200
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": "peer_banned"
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"banPeer","params":{"host":"10.0.0.2","port":12346}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid or missing parameters
- `-32603` — peer manager unavailable

---

### `unbanPeer`

Remove a ban for a peer.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `host` | string | yes | Peer hostname or IP |
| `port` | integer | yes | Peer P2P port |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "unbanPeer",
  "params": {
    "host": "10.0.0.2",
    "port": 12346
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": "peer_unbanned"
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"unbanPeer","params":{"host":"10.0.0.2","port":12346}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid or missing parameters
- `-32603` — peer manager unavailable
- `-32006` — peer is not banned

---

## Node

### `getNodeStatus`

Get comprehensive node health and state information.

**Parameters:** None.

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getNodeStatus"
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "chainLength": 42,
    "chunkCount": 1,
    "syncState": "idle",
    "currentDifficulty": 2,
    "inboundPeers": 0,
    "outboundPeers": 1,
    "nodeUuid": "abc123-..."
  }
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getNodeStatus"}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

---

### `getChainLength`

Get the total number of blocks in the chain.

**Parameters:** None.

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getChainLength"
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": "42"
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getChainLength"}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

---

### `getChunkCount`

Get the number of chunk files.

**Parameters:** None.

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getChunkCount"
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": "1"
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getChunkCount"}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

---

## Merkle

### `getInclusionProof`

Get a Merkle inclusion proof for a specific entry within a block.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `blockIndex` | integer | yes | Block index |
| `entryIndex` | integer | yes | Entry index within the block |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getInclusionProof",
  "params": {
    "blockIndex": 1,
    "entryIndex": 0
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "blockIndex": 1,
    "entryIndex": 0,
    "leafHash": "a1b2c3...",
    "merkleRoot": "d4e5f6...",
    "proof": [
      {
        "hash": "789abc...",
        "isLeft": true
      }
    ]
  }
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getInclusionProof","params":{"blockIndex":1,"entryIndex":0}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid parameters
- `-32001` — block not found
- `-32002` — entry not found

---

### `verifyInclusionProof`

Verify a Merkle inclusion proof against a block's Merkle root.

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `blockIndex` | integer | yes | Block index |
| `leafHash` | string | yes | SHA-256 hash of the leaf to verify |
| `proof` | array | yes | Proof path — array of `{"hash": "...", "isLeft": bool}` objects |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "verifyInclusionProof",
  "params": {
    "blockIndex": 1,
    "leafHash": "a1b2c3...",
    "proof": [
      {
        "hash": "789abc...",
        "isLeft": true
      }
    ]
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "blockIndex": 1,
    "merkleRoot": "d4e5f6...",
    "computedRoot": "d4e5f6...",
    "valid": true
  }
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"verifyInclusionProof","params":{"blockIndex":1,"leafHash":"a1b2c3...","proof":[{"hash":"789abc...","isLeft":true}]}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid parameters
- `-32001` — block not found

---

### `getBlockHeader`

Get a block's header (metadata without entries).

**Parameters:**

| Name | Type | Required | Description |
|------|------|----------|-------------|
| `blockIndex` | integer | yes | Block index |

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "getBlockHeader",
  "params": {
    "blockIndex": 1
  }
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": {
    "index": 1,
    "timestamp": 1712937600,
    "prevHash": "0000...",
    "hash": "00ab...",
    "nonce": 42,
    "difficulty": 1,
    "merkleRoot": "a1b2..."
  }
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"getBlockHeader","params":{"blockIndex":1}}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32602` — invalid parameters
- `-32001` — block not found

---

## Sync

### `requestSync`

Trigger a chain synchronization with a connected peer.

**Parameters:** None.

**Request:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "method": "requestSync"
}
```

**Response:**

```json
{
  "jsonrpc": "2.0",
  "id": 1,
  "result": "sync_started"
}
```

**Example:**

```bash
echo '{"jsonrpc":"2.0","id":1,"method":"requestSync"}' | \
  openssl s_client -connect localhost:12345 -CAfile ca.pem -quiet 2>/dev/null
```

**Errors:**
- `-32001` — sync already in progress
- `-32003` — no peer connected

---

## Error Codes

### Standard JSON-RPC Errors

| Code | Name | Description |
|------|------|-------------|
| `-32700` | Parse Error | Invalid JSON received |
| `-32600` | Invalid Request | Not a valid JSON-RPC 2.0 message |
| `-32601` | Method Not Found | Unknown method name |
| `-32602` | Invalid Params | Missing or invalid method parameters |
| `-32603` | Internal Error | Server-side error (e.g., peer manager unavailable) |

### Application-Specific Errors

| Code | Name | Description |
|------|------|-------------|
| `-32000` | Mining Timeout | Block mining exceeded the configured timeout |
| `-32001` | Not Found / Sync Conflict | Block or resource not found, or sync already in progress, or node is syncing |
| `-32002` | Entry Not Found | Stream entry index out of bounds within a block |
| `-32003` | Resource Unavailable | No peer connected, outbound limit reached, or stream not permitted |
| `-32004` | Already Exists | Stream or resource already exists, or peer is banned |
| `-32005` | Peer Not Found | Specified peer is not connected |
| `-32006` | Not Banned | Peer is not currently banned |
