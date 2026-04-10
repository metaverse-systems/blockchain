# JSON-RPC Contract: Block Propagation & Validation on Receipt

**Feature**: 005-block-propagation
**Date**: 2026-04-10
**Base contract**: [004 JSON-RPC contract](../../004-peer-discovery/contracts/json-rpc.md)

This document describes **only the changes** to the JSON-RPC contract introduced by block propagation.

## Modified Methods

### `addBlock` — Updated Behavior

The `addBlock` method already exists. Its request/response format is unchanged. The behavioral change is:

**Before (004)**: Block is mined, appended locally, saved to chunk, and returned.

**After (005)**: After the block is mined, appended locally, and saved to chunk, the node broadcasts the block to all connected peers via P2P. The RPC response is returned immediately after the local append — it does not wait for peer acknowledgment.

**Request** (unchanged):
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "method": "addBlock",
  "params": {
    "data": "payload string",
    "keys": ["key1", "key2"]
  }
}
```

**Response (success)** (unchanged):
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": "{\"index\":42,\"timestamp\":1712700000,\"data\":\"payload string\",\"prevHash\":\"abc...\",\"hash\":\"def...\",\"nonce\":12345,\"difficulty\":3}"
}
```

**New behavior**: After the response is queued for write, `PeerManager::broadcast_block(block)` is called. This is fire-and-forget — broadcast errors do not affect the RPC response.

## No New Methods

This feature does not introduce new JSON-RPC methods. Block propagation is automatic over the P2P protocol.
