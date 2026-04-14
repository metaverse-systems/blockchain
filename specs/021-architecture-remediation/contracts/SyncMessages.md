# Contract: SyncMessages (Wire Format)

**Source**: `src/network/SyncMessages.hpp`  
**Consumers**: `PeerClient`, `PeerServer` (Boost.Serialization binary archives over TLS)

## SyncQuery (Unchanged)

```
{
  local_chain_height: uint64_t   // Requester's current chain height
}
```

## SyncResponse (Modified)

```
{
  total_chain_height: uint64_t   // Responder's total chain height
  start_index: uint64_t          // First block index in this batch (NEW, replaces chunk_index)
  blocks: vector<Block>          // Blocks in this batch
}
```

### Changes from Previous Format

| Field | Old | New |
|-------|-----|-----|
| `chunk_index` | Present — storage-specific chunk number | **REMOVED** |
| `start_index` | Absent | **ADDED** — first block index in batch, storage-agnostic |

### Batching Strategy

- `PeerServer` sends blocks in batches of up to 100 (matching the internal chunk size for efficiency, but this is an implementation detail not exposed in the wire format).
- Batch boundaries are computed by block index range, not by chunk identity.
- `start_index` allows the receiver to validate that batches arrive in order.

### Backward Compatibility

- None required. All nodes update simultaneously per clarification decision.
- The Boost.Serialization archive format changes (field removed + field added), making the new format binary-incompatible with the old one.

## Serialization

Boost.Serialization binary archive format via `serialize()` template method. Fields serialized in declaration order:

```
ar & total_chain_height;
ar & start_index;
ar & blocks;
```
