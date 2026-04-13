# Data Model: 018-audit-bug-security-fixes

**Date**: 2026-04-13  
**Spec**: [spec.md](spec.md)

## Summary

This feature modifies no data models, schemas, or persistent formats. All
changes are behavioral fixes to existing code paths. The entities below
document existing structures referenced by the fixes.

---

## Existing Entities (no changes)

### Block

The fundamental chain unit. No field changes.

| Field | Type | Description |
|-------|------|-------------|
| index | `size_t` | Block position in the chain |
| timestamp | `time_t` | Creation timestamp |
| prevHash | `std::string` | Hash of the previous block |
| hash | `std::string` | SHA-256 hash of this block |
| merkleRoot | `std::string` | Merkle root of entries |
| entries | `std::vector<StreamEntry>` | Key-value stream data |
| nonce | `uint64_t` | Proof-of-work nonce |
| difficulty | `uint32_t` | Mining difficulty target |

### SyncResponse (wire format — unchanged)

| Field | Type | Description |
|-------|------|-------------|
| total_chain_height | `uint64_t` | Sender's total block count |
| chunk_index | `uint64_t` | Which chunk this batch represents |
| blocks | `std::vector<Block>` | Blocks in this batch |

### PeerAddress

| Field | Type | Description |
|-------|------|-------------|
| host | `std::string` | Hostname or IP address |
| port | `uint16_t` | Port number, valid range [1, 65535] |

---

## Interface Changes

### ChainPersistence::validateChunk()

**Before**: `bool validateChunk(size_t chunkIndex, const ConsensusConfig& config)`
**After**: `std::optional<ChunkHandler> validateChunk(size_t chunkIndex, const ConsensusConfig& config)`

Returns the loaded and validated chunk on success, `std::nullopt` on failure.
Only caller is `recoverChain()`.

---

## State Transitions

No new state machines. The sync handler's block-append loop transitions from
"skip known blocks" to "append new blocks via `bc.appendBlock()`" within the
existing sync state machine.

---

## Validation Rules (new/changed)

| Rule | Location | Description |
|------|----------|-------------|
| Port range [1, 65535] | `parsePeerKey()` | Reject ports outside valid range |
| Port range [1, 65535] | `main.cpp` seed parsing | Reject invalid CLI seed ports |
| Block index < chain length | `RpcServer` getBlockByIndex | Reject out-of-range RPC queries |
| Overlap hash match | `handle_sync_response()` | Abort batch if overlap block hashes mismatch |
| Empty batch → end-of-sync | `handle_sync_response()` | Stop requesting if batch is empty |
