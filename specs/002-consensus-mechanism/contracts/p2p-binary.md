# P2P Binary Protocol Contract: Consensus Mechanism Changes

**Feature**: 002-consensus-mechanism  
**Date**: 2026-04-10  
**Base contract**: [001 P2P binary contract](../../001-code-constitution-audit/contracts/p2p-binary.md)

This document describes **only the changes** to the P2P binary protocol introduced by the consensus mechanism.

## Block Serialization Changes

The `BLOCK` packet body (Boost.Serialization binary archive) includes all `Block` fields:

| Field | Type | Description |
|-------|------|-------------|
| `index` | `size_t` | Sequential block number |
| `timestamp` | `uint64_t` | Unix epoch seconds |
| `data` | `std::string` | Opaque payload |
| `prevHash` | `std::string` | SHA-256 hash of previous block |
| `hash` | `std::string` | SHA-256 hash of this block |
| `nonce` | `uint64_t` | Proof-of-work solution |
| `difficulty` | `uint32_t` | Leading zero bits required at time of mining |

No serialization versioning is used — all nodes run the same version.

## Block Validation on Receipt

When a `BLOCK` packet is received via P2P, the node MUST now validate the block against consensus rules before any further processing:

1. Verify `hash == calculateHash()` (includes nonce and difficulty in hash input)
2. Verify `checkLeadingZeroBits(hash, difficulty) == true` (valid proof-of-work)
3. Verify `difficulty == expectedDifficultyForHeight(index)` (correct difficulty for block height)
4. Verify `timestamp <= now + maxFutureTimestamp` (not too far in the future)
5. Verify chain linkage (`prevHash`, `index`) against local chain

Invalid blocks MUST be rejected with a log message. Valid blocks are candidates for chain extension or fork resolution.

## Changes in This Feature

- **FR-001**: Received blocks validated against consensus rules (PoW, difficulty, timestamp).
- **FR-003**: Block binary format includes nonce and difficulty fields.
- **FR-012**: Consensus validation failures logged with descriptive messages.
