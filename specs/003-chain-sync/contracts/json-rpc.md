# JSON-RPC Contract: Chain Synchronization

**Feature**: 003-chain-sync
**Date**: 2026-04-10
**Base contract**: [002 JSON-RPC contract](../../002-consensus-mechanism/contracts/json-rpc.md)

This document describes **only the changes** to the JSON-RPC contract introduced by chain synchronization.

## New Methods

### `requestSync`

Manually triggers chain synchronization with the connected peer. Returns immediately — sync runs asynchronously.

**Request**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "requestSync",
  "params": {}
}
```

**Response (success)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "sync_started"
}
```

**Response (already syncing)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32002,
    "message": "Sync already in progress"
  }
}
```

**Response (no peer connected)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32003,
    "message": "No peer connected"
  }
}
```

## Modified Methods

### `addBlock` — Blocked During Sync

When a sync operation is in progress, `addBlock` returns an error instead of mining and adding a block.

**Request**: Unchanged.

**Response (during sync)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32001,
    "message": "Node is syncing",
    "data": "addBlock is unavailable while chain synchronization is in progress"
  }
}
```

All other behavior of `addBlock` is unchanged when not syncing.

### `getBlockByIndex`, `getBlocksByKeys` — Unchanged

Read-only methods continue to work normally during sync. They return whatever blocks are currently available in the local chain (which may be incomplete during sync).

## New Error Codes

| Code | Message | When |
|------|---------|------|
| -32001 | Node is syncing | `addBlock` called during active sync |
| -32002 | Sync already in progress | `requestSync` called while sync is already running |
| -32003 | No peer connected | `requestSync` called but no P2P peer is connected |
