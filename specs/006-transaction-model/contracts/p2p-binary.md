# P2P Binary Protocol Contract: 006 — Stream Entry Changes

**Protocol**: Custom binary over mutual TLS  
**Port**: 12346 (default)  
**Base contract**: [001 P2P binary contract](../../001-code-constitution-audit/contracts/p2p-binary.md)

## Changes

### BLOCK Packet (type 0) — Updated Payload

The `BLOCK` packet body is a Boost.Serialization binary archive of the `Block` struct. With this feature, the `data` field is removed and replaced by `entries` (`std::vector<StreamEntry>`). No serialization versioning is used — the struct remains at version 0.

**Updated format**:
```
Block { index, timestamp, prevHash, hash, nonce, difficulty, entries }
```

Where `entries` is a `std::vector<StreamEntry>`, each containing `stream`, `key`, and `data` strings.

Since there are no existing blockchains, all nodes must run the updated code. There is no mixed-version compatibility concern.

### Validation for Received Blocks

When a node receives a `BLOCK` packet with stream entries, the following additional validation is performed before accepting the block:

1. Each `StreamEntry.stream` must match `^[a-zA-Z0-9_-]{1,256}$`
2. Each `StreamEntry.key` must be non-empty
3. Each `StreamEntry.data` must not exceed 128 MB

If any entry fails validation, the entire block is rejected. Per-node stream permission settings are **not** applied to P2P blocks — only structural validation is enforced.

### No New Packet Types

No new packet types are introduced. Stream entries ride within the existing `BLOCK` packet as part of the `Block` struct.
