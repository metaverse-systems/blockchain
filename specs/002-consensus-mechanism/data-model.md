# Data Model: Consensus Mechanism

**Feature**: 002-consensus-mechanism  
**Date**: 2026-04-10

## Entities

### Block (extended)

The existing `Block` struct gains two new fields for consensus.

| Field | Type | Description | Existing? |
|-------|------|-------------|-----------|
| `index` | `size_t` | Sequential block number | Yes |
| `timestamp` | `uint64_t` | Unix epoch seconds when block was created | Yes |
| `data` | `std::string` | Opaque payload | Yes |
| `prevHash` | `std::string` | SHA-256 hash of previous block | Yes |
| `hash` | `std::string` | SHA-256 hash of this block (computed from all fields) | Yes |
| `nonce` | `uint64_t` | Proof-of-work solution (incremented during mining) | **New** |
| `difficulty` | `uint32_t` | Required leading zero bits at time of mining | **New** |

**Default values**: `nonce = 0`, `difficulty = 0` (used by genesis block).

**Hash computation change**: `calculateHash()` must include `nonce` and `difficulty` in the hash input:
```
hash = SHA256(index || timestamp || data || prevHash || nonce || difficulty)
```

**Serialization**: No versioning needed — there are no existing blockchain data files. All fields serialized unconditionally at version 0.

**Validation rules**:
- `index == previousBlock.index + 1`
- `prevHash == previousBlock.hash`
- `hash == calculateHash()` (recomputed)
- `checkLeadingZeroBits(hash, difficulty) == true` (unless genesis)
- `difficulty == expectedDifficultyForHeight(index)` (matches chain's difficulty schedule)
- `timestamp <= now + MAX_FUTURE_TIMESTAMP` (default 120s)


### ConsensusConfig

Configurable parameters for the consensus mechanism. Read from environment variables at startup.

| Parameter | Type | Default | Env Variable |
|-----------|------|---------|-------------|
| `targetBlockInterval` | `uint32_t` (seconds) | 10 | `BLOCKCHAIN_TARGET_INTERVAL` |
| `adjustmentWindow` | `uint32_t` (blocks) | 10 | `BLOCKCHAIN_ADJUST_WINDOW` |
| `maxAdjustmentFactor` | `double` | 4.0 | `BLOCKCHAIN_MAX_ADJUST_FACTOR` |
| `minDifficulty` | `uint32_t` (bits) | 1 | `BLOCKCHAIN_MIN_DIFFICULTY` |
| `maxDifficulty` | `uint32_t` (bits) | 16 | `BLOCKCHAIN_MAX_DIFFICULTY` |
| `initialDifficulty` | `uint32_t` (bits) | 1 | `BLOCKCHAIN_INITIAL_DIFFICULTY` |
| `miningTimeout` | `uint32_t` (seconds) | 30 | `BLOCKCHAIN_MINING_TIMEOUT` |
| `maxFutureTimestamp` | `uint32_t` (seconds) | 120 | `BLOCKCHAIN_MAX_FUTURE_TIMESTAMP` |
| `maxReorgDepth` | `uint32_t` (blocks) | 100 | `BLOCKCHAIN_MAX_REORG_DEPTH` |

### Chain State (derived, not persisted)

Computed from the chain at runtime. Not serialized — rebuilt on startup from block data.

| Field | Type | Description |
|-------|------|-------------|
| `currentDifficulty` | `uint32_t` | Active difficulty for next block |
| `blocksSinceAdjustment` | `uint32_t` | Blocks mined since last difficulty change |
| `lastAdjustmentTimestamp` | `uint64_t` | Timestamp of the first block in current adjustment window |

## Relationships

```
Block(N).prevHash ──references──> Block(N-1).hash
Block(N).difficulty ──derived-from──> ConsensusConfig + recent block timestamps
ConsensusConfig ──read-at-startup──> Environment variables / .env file
ChunkHandler ──contains──> up to 100 Blocks (existing, unchanged)
Blockchain.chain ──ordered-vector-of──> ChunkHandler (existing, unchanged)
```

## State Transitions

### Block Lifecycle

```
[Data submitted] → [Mining started] → [Nonce incremented in loop]
       │                                       │
       │                              [Hash checked against difficulty]
       │                                       │
       │                              ┌────────┴────────┐
       │                              │                  │
       │                        [Meets target]    [Doesn't meet]
       │                              │                  │
       │                              ▼              [Continue loop]
       │                     [Block created]             │
       │                              │          [Timeout exceeded?]
       │                              │                  │
       │                              │            [Return error]
       │                              ▼
       │                     [Validation passed]
       │                              │
       │                              ▼
       │                     [Appended to chain]
       │                              │
       │                              ▼
       │                     [Chunk saved to disk]
```

### Difficulty Adjustment

```
[Block N added] → [N % adjustmentWindow == 0?]
                         │
                    ┌─────┴─────┐
                    │           │
                   No          Yes
                    │           │
                    │     [Compute time for last window]
                    │           │
                    │     [ratio = expected / actual]
                    │           │
                    │     [Clamp ratio to [1/maxFactor, maxFactor]]
                    │           │
                    │     [newDifficulty = current + round(log2(ratio))]
                    │           │
                    │     [Clamp to [minDifficulty, maxDifficulty]]
                    │           │
                    │     [Update currentDifficulty]
                    │           │
                    └─────┬─────┘
                          │
                    [Continue]
```

### Chain Replacement

```
[Candidate chain received] → [Length > current + reorgDepth check]
                                      │
                               ┌──────┴──────┐
                               │             │
                          [Too short      [Longer &
                           or too deep]    within depth]
                               │             │
                          [Reject]     [Validate every block]
                                             │
                                      ┌──────┴──────┐
                                      │             │
                                 [Invalid]     [All valid]
                                      │             │
                                 [Reject]     [Replace chain]
                                                    │
                                              [Rebuild keyIndexMap]
```
