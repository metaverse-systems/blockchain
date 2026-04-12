# Research: Code Audit Remediation

**Feature**: 016-audit-remediation  
**Date**: 2026-04-12  
**Purpose**: Resolve all technical unknowns before Phase 1 design

---

## R1: getChainBlockCount() Fix Strategy

**Decision**: Return `totalBlockCount_` directly, matching `getChainLength()` (line 454–457) which already does this correctly.

**Rationale**: The cached `totalBlockCount_` is maintained by `publish()` (line 23), `appendBlock()` (line 253), `recoverChain()` (line 684), and `replaceChain()` (line 858). The cache is already correct — only `getChainBlockCount()` ignores it by walking freed chunks. The fix is a one-line change.

**Alternatives considered**:
- Remove `getChainBlockCount()` entirely and have callers use `getChainLength()` → Rejected because `getChainBlockCount()` is used by multiple callers through `IBlockchain` and renaming would be a broader interface change out of scope.

---

## R2: Single-Threaded io_context Enforcement

**Decision**: Add a `std::atomic<int>` thread-count guard at the `io_context::run()` call site in `main.cpp` (line 215). Assert that only one thread calls `run()`. Add a documentation comment explaining the threading model.

**Rationale**: `main.cpp` already calls `io_context.run()` on only the main thread (line 215). All async handlers (PeerManager, BlockPropagation, RpcServer, Blockchain periodic save) rely on implicit serialization from single-threaded `io_context`. Adding an explicit guard prevents future developers from naively adding `std::thread` pools.

**Implementation pattern**:
```cpp
// All async handlers in this application assume single-threaded
// io_context execution. Do NOT add threads or call io_context.run()
// from multiple threads without first adding strand/mutex protection
// to BlockPropagation, PeerManager, Blockchain, and RpcServer.
std::atomic<int> run_count{0};
assert(++run_count == 1 && "io_context::run() must be called from exactly one thread");
io_context.run();
```

**Alternatives considered**:
- `boost::asio::strand` wrappers on all shared state → Rejected: large blast radius for a remediation feature, risks introducing deadlocks/ordering bugs. Better done as a dedicated future feature after architecture refactoring (§6.2–6.4).
- `std::mutex` on all shared containers → Rejected: same reasons as strand approach, plus lock contention on hot paths.

---

## R3: Difficulty Cache Design

**Decision**: Add a `std::unordered_map<size_t, uint32_t> difficultyCache_` member to `Blockchain`, keyed by adjustment boundary height, storing the difficulty at that boundary. Populated lazily during `getDifficultyForHeight()` and invalidated on `replaceChain()`.

**Rationale**: `getDifficultyForHeight()` (lines 920–955) iterates from `adjustmentWindow` up to `height` in steps of `adjustmentWindow`. The difficulty at each boundary depends only on the blocks at that boundary — which don't change once mined. A cache keyed by boundary height makes subsequent calls O(1) for already-computed boundaries.

**Invalidation rules**:
- `replaceChain()`: Clear entire cache (chain history changed)
- `recoverChain()`: Clear and rebuild from validated chain
- Normal `publish()`/`appendBlock()`: No invalidation needed (new boundaries are computed and cached on first access)

**Persistence**: Not persisted to disk. Rebuilt lazily on node startup as difficulty is queried. The cache is small (one entry per adjustment window) and cheap to recompute.

**Alternatives considered**:
- Persist cache in a separate file → Rejected: adds storage complexity for minimal benefit. Cache rebuilds are fast since the fix also includes chunk retention (R4).
- Store difficulty in block headers → Rejected: changes the block serialization format, which would require migration of all existing chain data.

---

## R4: Chunk Retention During Multi-Access Operations

**Decision**: Add a `std::set<size_t> retainedChunks_` member and `retainChunk(size_t)`/`releaseChunks()` methods to `Blockchain`. When a chunk is in the retained set, `freeChunk()` becomes a no-op for that chunk. Callers like `getDifficultyForHeight()` and `recoverChain()` call `releaseChunks()` when done.

**Rationale**: `getBlockByIndex()` (lines 271–295) loads a chunk then immediately frees it if `wasEmpty && chunkIndex + 1 < chain.size()`. In `getDifficultyForHeight()`, this causes the same chunk to be loaded and freed for each adjustment boundary that falls within it. The retain set prevents freeing during multi-access operations without changing the `getBlockByIndex()` API.

**Implementation pattern**:
```cpp
// In getDifficultyForHeight():
ChunkRetainGuard guard(*this);  // RAII — calls releaseChunks() on destruction
for (size_t boundary = ...; ...; ...) {
    Block first = getBlockByIndex(windowStart);   // chunk stays loaded
    Block last = getBlockByIndex(windowEnd);      // same chunk not reloaded
}
// guard destructor frees retained chunks
```

**Alternatives considered**:
- LRU cache with configurable size → Rejected: more complex, configuration burden, and the retain-set approach is simpler and precisely scoped to the problem.
- Remove the free-after-use entirely → Rejected: chunks must be freed for memory management on long chains. The current approach of freeing after use is correct; the bug is freeing *immediately within a multi-access loop*.

---

## R5: Merkle Root Verify-Then-Cache for Received Blocks

**Decision**: Add a new Block constructor overload that accepts a pre-computed merkle root and hash. On construction, it verifies the merkle root against entries. If verification passes, the provided values are used. If it fails, the block is rejected (constructor throws).

**Rationale**: The current parameterised constructor (Block.cpp lines 19–32) always recomputes merkle root and hash from entries. For received blocks during sync, this wastes CPU proportional to entry count per block. A verify-then-cache constructor pays the verification cost once (same as recomputation) but establishes a pattern where the block's integrity is confirmed without redundant work on subsequent operations.

**Constructor signature**:
```cpp
Block(size_t index, uint64_t time, std::string prev_hash,
      std::vector<StreamEntry> entries, uint64_t nonce,
      uint32_t difficulty, std::string merkleRoot, std::string hash);
```

**Alternatives considered**:
- Skip verification entirely (trust sender) → Rejected: violates blockchain trust model where peers are untrusted.
- Verify lazily on first use → Rejected: deferred verification could allow invalid blocks to propagate before detection.

---

## R6: IPv6-Safe Peer Key Parsing

**Decision**: Extract a `parsePeerKey(const std::string& key)` utility function into `utils.hpp`/`utils.cpp` that handles both IPv4 (`host:port`) and IPv6 (`[host]:port`) formats. Use `rfind(':')` for IPv4, bracket-aware parsing for IPv6.

**Rationale**: The current `sender_key.find(':')` (BlockPropagation.cpp lines 122 and 168) splits at the first colon, which breaks for IPv6 addresses like `[::1]:8333`. Using `rfind(':')` finds the last colon, which is the port separator for both `192.168.1.1:8333` and `[::1]:8333`.

**Implementation pattern**:
```cpp
std::pair<std::string, uint16_t> parsePeerKey(const std::string& key) {
    if (key.empty()) throw std::invalid_argument("empty peer key");
    auto last_colon = key.rfind(':');
    if (last_colon == std::string::npos || last_colon == 0)
        throw std::invalid_argument("malformed peer key: " + key);
    std::string host = key.substr(0, last_colon);
    // Strip brackets from IPv6
    if (host.front() == '[' && host.back() == ']')
        host = host.substr(1, host.size() - 2);
    uint16_t port = static_cast<uint16_t>(std::stoi(key.substr(last_colon + 1)));
    return {host, port};
}
```

**Alternatives considered**:
- Use Boost.Asio endpoint parsing → Rejected: adds Asio dependency to a string utility, and the keys are not full endpoint objects.
- Require all keys to be IPv4 → Rejected: constitution §VII requires cross-platform support, and IPv6 is standard.

---

## R7: Pending Pool Data Structure

**Decision**: Replace `std::unordered_map<std::string, PendingEntry>` with a combined `std::unordered_map<std::string, PendingEntry>` (for O(1) hash lookup) + `std::deque<std::string>` (for O(1) insertion-ordered eviction).

**Rationale**: The current eviction (BlockPropagation.cpp lines 57–70) performs O(n) linear scan to find the oldest `inserted_at`. A deque maintains insertion order, so `front()` is always the oldest. The map provides O(1) lookup by block hash for deduplication.

**Eviction algorithm**:
```
1. If pool full: pop front of deque → get hash → erase from map
2. Insert: push_back on deque, insert into map
3. Lookup (dedup): check map.contains(hash)
4. Expire: walk from front of deque while expired, erase from both
```

**Alternatives considered**:
- `std::map<time, hash>` → Rejected: O(log n) vs O(1), and time collisions require tie-breaking.
- Boost.Multi-index container → Rejected: adds complexity and Boost.Multi-index is not in the approved dependency set.

---

## R8: Chunk Filename Utility

**Decision**: Add `std::string chunkFilename(size_t index)` to `utils.hpp`/`utils.cpp` returning `"chunk_" + zero-padded-6-digit-index + ".dat"`. Replace all 8 inline constructions.

**Rationale**: The pattern `"chunk_" << setfill('0') << setw(6) << index << ".dat"` appears 8 times across `Blockchain.cpp` and `Chunk.cpp`. A single utility eliminates duplication and provides a single point of change for the filename format.

**Alternatives considered**:
- Static method on Chunk class → Rejected: `Blockchain.cpp` would need to include `Chunk.hpp` for the utility, creating unnecessary coupling. A free function in `utils` is more appropriate.

---

## R9: RPC Error Response Helper

**Decision**: Add `makeJsonRpcError(nlohmann::json id, int code, const std::string& message, nlohmann::json data = nullptr)` and `makeJsonRpcResult(nlohmann::json id, nlohmann::json result)` static methods to `RpcServer`. Replace all 9 error-response methods.

**Rationale**: All 9 methods (RpcServer.cpp lines 720–816) repeat `response["jsonrpc"] = "2.0"` and `response["id"] = id` verbatim. Two helper methods (error + result) cover all variants.

**Alternatives considered**:
- Free functions in `utils` → Rejected: RPC response construction is specific to `RpcServer` and shouldn't be in general utilities.
- Template method with overloads → Rejected: overcomplicated for what is a simple JSON construction pattern.

---

## R10: Unified Peer Sending

**Decision**: Replace `broadcast_block()` and `relay_block()` with a single `send_to_peers(const Block& block, const std::string& exclude_key = "")` method. When `exclude_key` is empty, sends to all peers (broadcast). When non-empty, excludes the specified peer (relay).

**Rationale**: The two methods (PeerManager.cpp lines 631–670) are ~95% identical. The only difference is the `if (key == exclude_key) continue;` guard. A default empty `exclude_key` parameter unifies both.

**Alternatives considered**:
- Keep both methods but have `broadcast_block()` call `relay_block("") ` → Considered but this approach is equivalent and using a single name is clearer.

---

## R11: dirty_ Flag Fix

**Decision**: Move `this->dirty_ = false` from line 71 (after chunk creation) to after the chunk is saved to disk. The flag should only be cleared when the chunk's state is safely persisted.

**Rationale**: Currently, when a chunk fills up in `publish()`, `dirty_` is set to `false` before the new block is appended. The subsequent append sets it back to `true`, but a crash between the two statements would leave `dirty_` incorrectly as `false`. Moving the clear to after the save ensures consistency.

**Alternatives considered**:
- Remove `dirty_ = false` entirely from `publish()` and only clear in `saveAllChunks()` → Viable but would mean more frequent saves. The targeted fix is simpler.

---

## R12: getStreamEntries() Deduplication

**Decision**: Extract the inner block-lookup loop into a lambda that takes a filter predicate. Both the with-key and without-key branches call the shared lambda with their respective predicates.

**Rationale**: The two branches (Blockchain.cpp lines 146–200) differ only in the entry filter (`e.key == key` vs. always true). A shared loop with a predicate parameter eliminates ~85% duplication.

**Alternatives considered**:
- Template function → Rejected: unnecessary for an internal method. A lambda captures the needed context naturally.

---

## R13: Shared Test Helpers

**Decision**: Create `tests/TestHelpers.hpp` providing:
- `createTestDir(const std::string& name)` → returns `std::filesystem::path`
- `cleanupTestDir(const std::filesystem::path&)`
- `defaultConsensusConfig()` → returns `ConsensusConfig` with difficulty=0
- `mineTestBlock(...)` → mines a block with the given parameters
- `buildValidChain(size_t length, ...)` → builds a valid chain of N blocks

**Rationale**: Audit §7.4 and §3.6 identified ~200 lines of duplicated test setup across 7–10 files. A shared header eliminates this and ensures consistent test configuration.

**Alternatives considered**:
- Catch2 fixtures → Could be used in combination, but a header-only utility is simpler and doesn't require fixture inheritance.
- Separate `.cpp` compilation unit → Rejected: header-only is simpler for test utilities and avoids Makefile changes beyond adding the header.

---

## R14: Packet Serialization Deduplication

**Decision**: Out of scope for this feature. The PeerClient/PeerServer serialization duplication (§3.3) involves two different socket types (`ssl_socket` vs. `socket`) and buffer ownership patterns (`shared_ptr` vs. member `write_buffer`). Unifying them requires a shared abstraction that touches the network layer architecture (§6.3), which is explicitly excluded.

**Rationale**: The two implementations (PeerClient.cpp lines 351–370 and PeerServer.cpp lines 272–300) share serialization logic but differ in buffer management and async_write targets. A clean unification requires either a template base class or a serialize-to-buffer utility with a polymorphic write step. This is better addressed when the network layer is refactored (§6.3).

**Alternatives considered**:
- Extract just the serialize-to-buffer step → Considered but deferred. The step that differs (async_write with different buffer ownership) is tightly coupled to the serialization step. Partial extraction provides limited value.
