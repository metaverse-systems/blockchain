# Research: Consensus Mechanism

**Feature**: 002-consensus-mechanism  
**Date**: 2026-04-10

## R1: Block Struct Extension with Consensus Fields

**Decision**: Add `nonce` and `difficulty` directly to the `Block` struct at version 0. No Boost.Serialization versioning needed.

**Rationale**: There are no existing blockchain data files to maintain backward compatibility with. The new fields (`nonce`, `difficulty`) are part of the base struct from the start. All serialization includes them unconditionally.

**Alternatives considered**:
- Boost.Serialization versioning (`BOOST_CLASS_VERSION`): Rejected — unnecessary since there are no legacy chunk files to deserialize.
- Separate consensus struct composed with Block: Rejected — adds indirection for no benefit.

**Pattern**:
```cpp
template<class Archive>
unsigned int serialize(Archive& ar, const unsigned int version)
{
    ar & index; ar & timestamp; ar & data; ar & prevHash; ar & hash;
    ar & nonce; ar & difficulty;
    return version;
}
```

## R2: Leading Zero Bits Check on SHA-256 Hex Output

**Decision**: Iterate hex characters counting complete zero digits (4 bits each), then check partial bits in the first non-zero hex digit.

**Rationale**: The existing `sha256()` function returns a 64-char lowercase hex string. Checking leading zero bits on a hex string is O(N) where N is typically 1–4 hex digits for the expected difficulty range. No hex-to-binary conversion is needed.

**Alternatives considered**:
- Convert hex to raw bytes and use `__builtin_clz`: Rejected — adds unnecessary conversion step; hex iteration is fast enough and more portable.
- 256-bit integer comparison (Bitcoin target model): Rejected — overkill for personal networks with 1–16 bit difficulty; leading-zero-bit model is simpler and sufficient.

**Algorithm**:
1. Count consecutive `'0'` hex chars → `zeroDigits * 4` bits.
2. If that already meets the requirement, return true.
3. Otherwise, parse the next hex digit as a nibble and count its leading zero bits (table: `0→4, 1→3, 2-3→2, 4-7→1, 8-f→0`).
4. Sum total leading zero bits and compare against difficulty.

## R3: Difficulty Adjustment Algorithm for Small Networks

**Decision**: Window-based adjustment with configurable parameters. Defaults: 10-block window, 4x max adjustment factor, difficulty range 1–16 bits.

**Rationale**: A 10-block window is small enough to react quickly (important for 2–10 node networks where a node going offline significantly changes hashrate) but large enough to smooth out natural variance. The 4x max factor prevents wild oscillation. Capping at 16 bits keeps mining feasible on commodity hardware (≈65k expected attempts per block), which is appropriate since PoW serves tamper-evidence, not Sybil resistance.

**Alternatives considered**:
- Bitcoin-style 2016-block window: Rejected — way too slow for a small personal network. Would take hours to respond to hashrate changes.
- Per-block (window=1): Rejected — too noisy; a single slow or fast block would whip difficulty around.
- Exponential moving average: Rejected — more complex to implement, harder to reason about; window-based is simpler and well-understood.

**Algorithm**:
1. Every `ADJUSTMENT_WINDOW` blocks, compute `expectedTime = targetInterval * windowSize`.
2. Compute `actualTime` from timestamps of first and last block in window.
3. `ratio = expectedTime / actualTime` (>1 means too fast, <1 means too slow).
4. Clamp ratio to `[1/MAX_FACTOR, MAX_FACTOR]`.
5. New difficulty = `current + round(log2(ratio))` (clamped to `[MIN_DIFFICULTY, MAX_DIFFICULTY]`).

**Default parameters**:
| Parameter | Default | Configurable |
|-----------|---------|-------------|
| Target block interval | 10s | Yes |
| Adjustment window | 10 blocks | Yes |
| Max adjustment factor | 4x | Yes |
| Min difficulty | 1 bit | Yes |
| Max difficulty | 16 bits | Yes |

## R4: Chain Replacement Strategy in Chunk Architecture

**Decision**: Full `vector<ChunkHandler>` replacement with lazy chunk loading. Do not diff individual chunks.

**Rationale**: In a 2–10 node trusted network, chain forks typically diverge by only a few blocks near the tip. Full vector replacement is an atomic operation that avoids partial-state inconsistency. Chunks are already lazily loaded on access (`getBlockByIndex` loads from disk only when needed), so replacing the vector doesn't trigger unnecessary disk I/O. The simplicity benefit far outweighs any marginal I/O savings from chunk-level diffing.

**Alternatives considered**:
- Diff-and-patch: Rejected — requires binary comparison of chunk contents to find the divergence point. In small networks with short chains (hundreds to low thousands of blocks), the overhead of diffing exceeds the cost of replacing the entire vector.
- Append-only with branch tracking: Rejected — adds significant complexity (DAG storage, branch pointers) that's unnecessary when the longest-chain rule provides a clear winner.

**Implementation approach**:
1. Validate the candidate chain fully (every block's PoW, linkage, timestamps).
2. Check candidate length > current length and reorg depth ≤ max.
3. Free currently loaded chunks.
4. Replace `chain` vector with candidate chain.
5. Rebuild `keyIndexMap` from the new chain (or invalidate and rebuild lazily).
6. Chunks load on-demand when blocks are accessed.
