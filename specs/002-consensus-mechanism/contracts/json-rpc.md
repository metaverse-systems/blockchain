# JSON-RPC Contract: Consensus Mechanism Changes

**Feature**: 002-consensus-mechanism  
**Date**: 2026-04-10  
**Base contract**: [001 JSON-RPC contract](../../001-code-constitution-audit/contracts/json-rpc.md)

This document describes **only the changes** to the JSON-RPC contract introduced by the consensus mechanism. The base contract remains authoritative for transport, authentication, and general protocol rules.

## Block Object Changes

The Block JSON object returned by all methods gains two new fields:

| Field | Type | Description |
|-------|------|-------------|
| `nonce` | `number` (uint64) | Proof-of-work solution |
| `difficulty` | `number` (uint32) | Leading zero bits required at time of mining |

**Note**: The genesis block has `nonce: 0` and `difficulty: 0`.

### Updated Block Object Example

```json
{
  "index": 5,
  "timestamp": 1712764800,
  "data": "example payload",
  "prevHash": "00a1b2c3...",
  "hash": "0004f8e2...",
  "nonce": 48291,
  "difficulty": 4
}
```

## Method Changes

### `addBlock` — Mining Behavior

`addBlock` now performs proof-of-work mining before returning. The call blocks until a valid nonce is found or the mining timeout is exceeded.

**Request**: Unchanged.

**Response (success)**: Block object now includes `nonce` and `difficulty` fields.

```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": {
    "index": 5,
    "timestamp": 1712764800,
    "data": "example payload",
    "prevHash": "00a1b2c3...",
    "hash": "0004f8e2...",
    "nonce": 48291,
    "difficulty": 4
  }
}
```

**Response (mining timeout)**:
```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "error": {
    "code": -32000,
    "message": "Mining timeout exceeded (30s)"
  }
}
```

### `getBlockByIndex` — Response Change

**Response**: Block object now includes `nonce` and `difficulty` fields. No request changes.

### `getBlocksByKeys` — Response Change

**Response**: Each Block object in the array now includes `nonce` and `difficulty` fields. No request changes.

## New Error Codes

| Code | Message | Condition |
|------|---------|-----------|
| -32000 | Mining timeout exceeded ({N}s) | Proof-of-work not found within configured timeout |

## Changes in This Feature

- **FR-002**: `addBlock` performs proof-of-work mining; response includes `nonce` and `difficulty`.
- **FR-003**: Block JSON object extended with `nonce` and `difficulty` fields.
- **FR-011**: Existing clients continue working — new fields are additive.
- **FR-012**: Consensus validation failures returned with descriptive error messages.
- **FR-013**: Mining timeout produces a JSON-RPC error code -32000.
