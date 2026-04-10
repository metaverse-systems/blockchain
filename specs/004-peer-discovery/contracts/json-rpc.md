# JSON-RPC Contract: Peer Discovery & Management

**Feature**: 004-peer-discovery
**Date**: 2026-04-10
**Base contract**: [003 JSON-RPC contract](../../003-chain-sync/contracts/json-rpc.md)

This document describes **only the changes** to the JSON-RPC contract introduced by peer discovery and management.

## New Methods

### `addPeer`

Adds a peer address and initiates an outbound connection. If the peer is already known, updates the entry and reconnects if disconnected.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "addPeer",
  "params": {
    "host": "192.168.1.10",
    "port": 12346
  }
}
```

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "peer_added"
}
```

**Response (at connection limit)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32003,
    "message": "Outbound connection limit reached"
  }
}
```

**Response (peer is banned)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32004,
    "message": "Peer is currently banned",
    "data": {
      "expires": 1712703600
    }
  }
}
```

**Response (invalid params)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32602,
    "message": "Invalid params: host and port are required"
  }
}
```

---

### `removePeer`

Disconnects from a peer (if connected) and removes it from the known peer list.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "2",
  "method": "removePeer",
  "params": {
    "host": "192.168.1.10",
    "port": 12346
  }
}
```

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "2",
  "result": "peer_removed"
}
```

**Response (peer not found)**:
```json
{
  "jsonrpc": "2.0",
  "id": "2",
  "error": {
    "code": -32005,
    "message": "Peer not found"
  }
}
```

---

### `listPeers`

Returns the current peer list with connection status for each peer.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "3",
  "method": "listPeers",
  "params": {}
}
```

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "3",
  "result": {
    "node_uuid": "550e8400-e29b-41d4-a716-446655440000",
    "discovery_enabled": true,
    "outbound_count": 3,
    "inbound_count": 1,
    "max_outbound": 8,
    "max_inbound": 32,
    "peers": [
      {
        "host": "192.168.1.10",
        "port": 12346,
        "node_uuid": "6ba7b810-9dad-11d1-80b4-00c04fd430c8",
        "state": "connected",
        "direction": "outbound",
        "last_seen": 1712700000,
        "error_count": 0
      },
      {
        "host": "10.0.0.5",
        "port": 12346,
        "node_uuid": "",
        "state": "disconnected",
        "direction": "none",
        "last_seen": 1712699000,
        "error_count": 2
      }
    ],
    "bans": [
      {
        "host": "10.0.0.99",
        "port": 12346,
        "reason": "excessive_errors",
        "expires": 1712703600
      }
    ]
  }
}
```

---

### `banPeer`

Manually bans a peer. Disconnects the peer if currently connected.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "4",
  "method": "banPeer",
  "params": {
    "host": "10.0.0.5",
    "port": 12346,
    "duration_seconds": 7200
  }
}
```

`duration_seconds` is optional. If omitted, uses the configured default (`ban_duration_seconds`). If set to `0`, the ban is permanent until manually lifted.

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "4",
  "result": "peer_banned"
}
```

---

### `unbanPeer`

Removes a ban on a peer, allowing it to reconnect.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "5",
  "method": "unbanPeer",
  "params": {
    "host": "10.0.0.5",
    "port": 12346
  }
}
```

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "5",
  "result": "peer_unbanned"
}
```

**Response (not banned)**:
```json
{
  "jsonrpc": "2.0",
  "id": "5",
  "error": {
    "code": -32006,
    "message": "Peer is not banned"
  }
}
```

## New Error Codes

| Code | Meaning |
|------|---------|
| -32003 | Outbound connection limit reached |
| -32004 | Peer is currently banned |
| -32005 | Peer not found in peer list |
| -32006 | Peer is not banned |

These extend the existing error codes from prior specs:

| Code | Meaning | Spec |
|------|---------|------|
| -32600 | Invalid JSON-RPC message | 001 |
| -32601 | Method not found | 001 |
| -32602 | Invalid params | 001 |
| -32001 | Mining timeout | 002 |
| -32002 | Sync in progress | 003 |

## Modified Methods

### `addBlock`

No change to the method itself, but peer management context applies: if a peer sends a block that fails validation, `PeerManager` increments the error count for that peer. This is internal behavior, not a contract change.

## Unchanged Methods

All existing methods (`addBlock`, `getBlockByIndex`, `getBlocksByKeys`, `requestSync`) remain unchanged.
