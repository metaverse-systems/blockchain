# Research: 018-audit-bug-security-fixes

**Date**: 2026-04-13  
**Spec**: [spec.md](spec.md)

## Summary

No NEEDS CLARIFICATION items. All fixes involve well-understood bugs in existing
code with clear patterns already present in the codebase.

---

## R1: appendBlock() pattern for sync handler (§2.1)

**Decision**: Call `bc.appendBlock(block)` inside the existing loop in
`handle_sync_response()`, after the skip guard, following the same pattern as
`BlockPropagation::appendReceivedBlock()`.

**Rationale**: `appendBlock()` is the canonical method for adding a validated
block to the chain. It handles chunk sizing, stream index updates, and
difficulty recalculation. `BlockPropagation::appendReceivedBlock()` already
demonstrates correct usage followed by `saveChunk()` and `saveKeys()`.

**Alternatives considered**:
- `replaceChain()` — too heavy; designed for full-chain replacement, not
  incremental sync of individual blocks.
- Direct chunk manipulation — bypasses `appendBlock()` bookkeeping (stream
  indexes, total block count, dirty flag).

**Additional considerations**: Per spec clarifications, the handler must also:
- Check for hash mismatch in overlap region before appending any blocks
- Handle empty response as end-of-sync

---

## R2: Bounds check pattern for getBlockByIndex RPC (§3.2)

**Decision**: Check `index >= bc.getChainLength()` before calling
`bc.getBlockByIndex(index)`, and return error code -32001 "Block not found".

**Rationale**: The `getBlockRange` handler at RpcServer.cpp:664 already uses
`bc.getChainLength()` for bounds checking. This is the established pattern.

**Alternatives considered**:
- Catch the exception from `getBlockByIndex()` — reactive rather than proactive;
  relies on implementation details of how nonexistent chunks are handled.
- Add bounds checking inside `Blockchain::getBlockByIndex()` — pushes RPC
  responsibility into the domain layer; the RPC handler knows what error format
  to return.

---

## R3: Seed node port validation in main.cpp (§3.1)

**Decision**: Wrap the `std::stoi()` call in a try/catch, validate the port is
in [1, 65535], and also handle the missing-colon case. Print error to stderr and
return 1.

**Rationale**: This follows the existing validation pattern in main.cpp where
`node_config.validate()` failures are caught and printed to stderr with exit code 1.

**Alternatives considered**:
- Reuse `parsePeerKey()` from utils.cpp — it does split host:port, but is
  designed for already-formatted peer keys (supports `[IPv6]:port` bracket
  notation). After adding port range validation to `parsePeerKey()` (§2.4 fix),
  it could be reused here. **Decision**: Yes, refactor main.cpp to use
  `parsePeerKey()` after it's hardened, avoiding duplicate parsing logic.

---

## R4: parsePeerKey() port validation (§2.4)

**Decision**: After `std::stoi()`, validate that the result is in [1, 65535].
Wrap both `std::stoi()` exceptions and the range check in a single
`std::invalid_argument` throw with a descriptive message.

**Rationale**: The function already throws `std::invalid_argument` for other
malformed inputs (empty key, no colon). Adding port range validation is
consistent.

**Alternatives considered**:
- Use `std::stoul()` instead of `std::stoi()` — avoids negative numbers but
  still needs range validation for values > 65535. Marginal benefit.
- Return `std::optional<std::pair<...>>` — changes the public API of an
  existing utility; all callers expect exceptions.

---

## R5: recoverChain() single-pass loading (§2.2)

**Decision**: Modify `validateChunk()` to return `std::optional<ChunkHandler>`
(the loaded chunk on success, `std::nullopt` on failure). In `recoverChain()`,
cache the returned chunk and reuse it for cross-chunk linkage and block counting.
Keep a `prevChunk` variable across loop iterations to avoid reloading chunk N-1.

**Rationale**: The three loads are: (1) inside `validateChunk()`, (2) for
cross-chunk linkage, (3) for block counting. Returning the validated chunk
from step 1 eliminates steps 2 and 3.

**Alternatives considered**:
- Internal caching inside `ChainPersistence` (LRU or map) — over-engineered for
  sequential access; the loop processes chunks 0, 1, 2, ... in order.
- Separate `loadAndValidateChunk()` method — essentially the same as returning
  from `validateChunk()`, but with a less clean interface.

**Interface change**: `validateChunk()` signature changes from
`bool validateChunk(size_t, const ConsensusConfig&)` to
`std::optional<ChunkHandler> validateChunk(size_t, const ConsensusConfig&)`.
The `recoverChain()` caller is the only call site.

---

## R6: getBlockByIndex resize bug (§2.3)

**Decision**: Change the `resize()` call to use a loop with `emplace_back` that
assigns the correct chunk ID to each new entry, matching the pattern already
used in `appendBlock()`.

**Rationale**: `appendBlock()` at Blockchain.cpp:237 already does:
```cpp
this->chain.emplace_back(ChunkHandler(this->chain.size(), this->blockchainPath));
```
This correctly uses `this->chain.size()` as the chunk ID for each new entry.

**Alternatives considered**:
- Fix the `resize()` prototype to use a lambda — `std::vector::resize` doesn't
  support per-element initialization.
- Post-resize fixup loop — sets IDs after resize, but the entries are
  constructed with wrong IDs first.
