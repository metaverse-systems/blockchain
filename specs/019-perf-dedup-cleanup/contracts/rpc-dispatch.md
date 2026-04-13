# JSON-RPC Contract: 019-perf-dedup-cleanup

**Date**: 2026-04-13

## Overview

This feature refactors the RPC dispatch mechanism. The JSON-RPC interface
exposed to external clients does **not change** — all 20 methods retain
identical request/response formats. This contract documents the existing
interface for verification that the refactor is behavior-preserving.

## Method Registry

All methods below must be present in the dispatch table after refactoring.
Each method returns a JSON-RPC 2.0 response.

| Method | Params Required | Success Response | Error Codes |
|--------|----------------|------------------|-------------|
| `publish` | `stream`, `key`; optional: `data`, `keys` | Block JSON | -32602 (invalid params), -32000 (mining timeout), -32001 (syncing), -32003 (stream not permitted) |
| `createStream` | `name` | Stream name | -32602, -32000, -32001 |
| `listStreams` | none | JSON array of streams | — |
| `getStreamEntries` | `stream` | JSON array of entries | -32602 |
| `getStreamEntry` | `stream`, `key` | Entry JSON | -32602 |
| `requestSync` | none | `"sync_started"` | -32002 (already syncing), -32003 (no peer) |
| `getBlockByIndex` | `index` | Block JSON | -32602, -32001 (not found) |
| `getBlocksByKeys` | `keys` | JSON array of blocks | -32602 |
| `addPeer` | `host`, `port` | Success message | -32602 |
| `removePeer` | `host`, `port` | Success message | -32602 |
| `listPeers` | none | JSON array of peers | — |
| `banPeer` | `host`, `port`; optional: `reason`, `duration` | Success message | -32602 |
| `unbanPeer` | `host`, `port` | Success message | -32602 |
| `getInclusionProof` | `blockIndex`, `entryIndex` | Proof JSON | -32602 |
| `verifyInclusionProof` | `proof` | Boolean result | -32602 |
| `getBlockHeader` | `index` | Header JSON | -32602 |
| `getNodeStatus` | none | Status JSON | — |
| `getBlockRange` | `start`, `end` | JSON array of blocks | -32602 |
| `getChainLength` | none | Length string | — |
| `getChunkCount` | none | Count string | — |

## Error Response Format

All error responses use the existing `errorMessage()` helper:

```json
{
  "jsonrpc": "2.0",
  "error": {
    "code": -32601,
    "message": "Invalid method: unknownMethod"
  },
  "id": 1
}
```

Standard error codes:
- `-32600`: Invalid JSON-RPC message
- `-32601`: Method not found
- `-32602`: Invalid parameters
- `-32000`: Mining timeout / internal error
- `-32001`: Block not found / sync in progress
- `-32002`: Sync already in progress
- `-32003`: No peer connected / stream not permitted

## Verification

After refactoring, every method in this table must produce byte-identical
JSON responses for the same input. The existing `rpc_integration_tests`
exercise the live RPC interface over SSL sockets and serve as the primary
regression gate.
