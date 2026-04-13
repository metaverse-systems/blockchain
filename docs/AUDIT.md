# Code Audit Report

**Date:** 2026-04-13  
**Scope:** Full codebase — `src/` (6,179 lines) and `tests/` (7,676 lines across 25 files)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Bugs](#2-bugs)
3. [Security](#3-security)
4. [Performance Issues](#4-performance-issues)
5. [Code Duplication](#5-code-duplication)
6. [Architecture Concerns](#6-architecture-concerns)
7. [Test Quality](#7-test-quality)
8. [Recommendations](#8-recommendations)

---

## 1. Executive Summary

This is a fresh audit following two rounds of remediation (016-audit-remediation
and 017-blockchain-module-split). The previous audit's 12 issues have all been
resolved: `getChainBlockCount()` returns `totalBlockCount_` directly, the
`dirty_` flag race is fixed, merkle root verify-then-cache constructors are in
place, `parsePeerKey()` handles IPv6, single-threaded io_context execution is
enforced at runtime, `chunkFilename()` is centralised, and `Blockchain.cpp`
has been split into four focused modules.

This audit found **4 bugs**, **2 security issues**, **5 performance issues**,
**2 duplication clusters**, **3 architecture concerns**, and **significant
test-quality weaknesses** that reduce confidence in the test suite.

| Category              | Count | Highest Severity |
|-----------------------|------:|------------------|
| Bugs                  |     4 | High             |
| Security issues       |     2 | High             |
| Performance issues    |     5 | Medium           |
| Code duplication      |     2 | Medium           |
| Architecture concerns |     3 | Medium           |
| Test quality issues   |     6 | High             |

---

## 2. Bugs

### 2.1 Sync never appends received blocks — HIGH

[PeerClient.cpp](../src/network/PeerClient.cpp#L267-L277)

`handle_sync_response()` validates each block in the response correctly, then
enters a loop that *skips* every block:

```cpp
for (auto &block : response.blocks) {
    if (block.index < local_height) {
        continue; // Already have this block
    }
}
```

The loop body only contains the `continue` guard — there is no `appendBlock()`
or `replaceChain()` call. Validated sync blocks are discarded silently, and
the local chain never advances. Subsequent "more chunks needed" logic then
re-requests the same chunk endlessly because `getChainBlockCount()` never
increases.

**Fix:** After the skip guard, call `bc.appendBlock(block)` for each new
block, following the same pattern as `BlockPropagation::appendReceivedBlock()`.

### 2.2 `recoverChain()` loads each chunk up to 3 times — MEDIUM

[ChainPersistence.cpp](../src/ChainPersistence.cpp#L208-L248)

In the non-fast-startup path, `validateChunk(i)` loads chunk `i` internally.
The cross-chunk check then loads chunks `i-1` **and** `i` again via separate
`ChunkHandler` temporaries. Finally, the block-counting loop loads chunk `i`
a third time. Each chunk is deserialized from disk up to 3 times.

**Fix:** Have `validateChunk()` return the loaded chunk (or cache it), and
reuse it for cross-chunk linkage and block counting.

### 2.3 `getBlockByIndex()` passes wrong index to `ChunkHandler` resize — LOW

[Blockchain.cpp](../src/Blockchain.cpp#L194)

```cpp
this->chain.resize(chunkIndex + 1, ChunkHandler(chunkIndex + 1, this->blockchainPath));
```

The prototype `ChunkHandler` passed to `resize()` uses `chunkIndex + 1` as
its chunk ID. If multiple slots are created (e.g., resizing from 2 to 5), all
intermediate entries get the same ID (`chunkIndex + 1`). In practice, these
entries are overwritten on load, so this has no runtime effect — but it is
incorrect.

### 2.4 `parsePeerKey()` does not validate port range — LOW

[utils.cpp](../src/utils.cpp#L172-L180)

`parsePeerKey()` calls `std::stoi()` on the port substring. If the value is
negative or > 65535, the cast to `uint16_t` silently truncates. Additionally,
`std::stoi()` throws `std::out_of_range` for very large integers and
`std::invalid_argument` for non-numeric strings — neither is caught with a
descriptive message.

**Fix:** Validate that the parsed integer is in [1, 65535] before casting.

---

## 3. Security

### 3.1 Seed node port parsing crashes on invalid input — HIGH

[main.cpp](../src/main.cpp#L100-L105)

```cpp
for (const auto &seed : cli.seed_nodes) {
    auto colon = seed.rfind(':');
    if (colon != std::string::npos) {
        PeerAddress addr;
        addr.host = seed.substr(0, colon);
        addr.port = static_cast<uint16_t>(std::stoi(seed.substr(colon + 1)));
```

`std::stoi()` on user-supplied CLI input without a `try`/`catch`. A
malformed `--seed-node` value like `host:abc` or `host:99999` crashes the
node with an uncaught exception. The port is also not range-checked against
[1, 65535].

**Fix:** Wrap in `try`/`catch`, validate range, and emit a user-friendly
error message.

### 3.2 `getBlockByIndex` RPC has no bounds check — MEDIUM

[RpcServer.cpp](../src/network/RpcServer.cpp#L339-L349)

The `getBlockByIndex` handler passes the user-supplied index directly to
`bc.getBlockByIndex()` without checking it against `bc.getChainLength()`.
If the index exceeds the chain length, the method attempts to load a
nonexistent chunk from disk, which throws an unhandled exception that crashes
the RPC session. The `getBlockRange` handler correctly bounds-checks
`startIndex` — this handler should do the same.

**Fix:** Return a `-32001 "Block not found"` error if `index >= bc.getChainLength()`.

---

## 4. Performance Issues

### 4.1 O(n) peer lookups — MEDIUM

`find_peer()`, `add_peer()`, `remove_peer()`, `is_banned()`, and
`get_non_banned_peer_addresses()` all perform linear scans over
`std::vector<PeerEntry>` and `std::vector<BanRecord>`.

With a configured maximum of 256 stored peers and `is_banned()` called on
every peer exchange, connection attempt, and block reception, switching to
`std::unordered_map<std::string, PeerEntry>` keyed by `host:port` would drop
lookups from O(n) to O(1).

### 4.2 RPC dispatch is a 21-branch `if`/`else` chain — MEDIUM

[RpcServer.cpp](../src/network/RpcServer.cpp#L74-L704)

Every request walks up to 21 string comparisons. A
`std::unordered_map<std::string, Handler>` dispatch table would be O(1) and
reduce the 700-line `do_read()` callback into individually testable handler
functions.

### 4.3 `recoverChain()` loads each chunk multiple times — MEDIUM

As described in §2.2, each chunk is deserialized from disk up to 3 times
during recovery. For a node with 1000+ chunks, this triples startup time.

### 4.4 String construction in log calls — LOW

Throughout the codebase, `logMessage("INFO", "Block #" + std::to_string(...) + ...)`
constructs the string even when the log level would suppress it. The
`logMessage()` function already filters by level, but the string allocation
happens at the call site.

**Fix:** A level-check macro or a lazy-evaluation wrapper would eliminate
unnecessary allocations.

### 4.5 `replaceChain()` loads entire candidate into memory — LOW

[Blockchain.cpp](../src/Blockchain.cpp#L486-L536)

`replaceChain()` accepts `const std::vector<Block> &candidateBlocks` — the
full chain. For very long chains this means the entire history must fit in RAM
simultaneously. A streaming/chunked replacement would bound memory usage.

---

## 5. Code Duplication

### 5.1 Packet serialization in PeerClient and PeerServer — MEDIUM

`PeerClient::send<T>()` ([PeerClient.cpp](../src/network/PeerClient.cpp#L352-L375)) and
`PeerServer::send_packet<T>()` ([PeerServer.cpp](../src/network/PeerServer.cpp#L271-L300))
share the same serialize → `PacketHeader` → `memcpy` → `async_write` pattern.
This could live in a shared utility or base class.

### 5.2 Test files still duplicate `mineTestBlock()` / `buildValidChain()` — LOW

Despite `TestHelpers.hpp` existing, `sync_tests.cpp`,
`block_propagation_tests.cpp`, `consensus_tests.cpp`, and
`chunk_persistence_tests.cpp` still define their own local versions of
`mineTestBlock()`, `buildValidChain()`, and temporary-directory helpers.

---

## 6. Architecture Concerns

### 6.1 `IBlockchain` interface is wide

[IBlockchain.hpp](../src/IBlockchain.hpp) exposes 28 methods including
persistence (`saveChunk`, `saveKeys`), mining (`publish`), querying
(`getStreamEntries`), and sync (`replaceChain`). Consumers that only need read
access (e.g. `RpcServer` for query endpoints) are coupled to the full
interface.

**Suggestion:** Split into `IChainReader` (query methods) and `IChainWriter`
(mutation methods). `RpcServer` depends only on `IChainReader` plus a small
`IChainWriter` for `publish` and `createStream`.

### 6.2 No separation between domain and network layers

`PeerClient`, `PeerServer`, and `BlockPropagation` directly call
`IBlockchain` methods. If the consensus rules or block format change, the
network layer must change too.

**Suggestion:** Introduce a thin service layer (e.g. `ChainService`) that
mediates between the network and domain layers. The network layer would submit
blocks to the service, which validates and delegates to `Blockchain`.

Additionally, the sync protocol currently leaks storage details: `SyncResponse`
includes a `chunk_index` field, coupling the wire format to the internal chunk
storage scheme. The `ChainService` should own the batching strategy so that
the network layer exchanges blocks only, with no awareness of chunks.

### 6.3 Error handling is inconsistent

| Pattern                | Used by                                    |
|------------------------|--------------------------------------------|
| Throw `std::runtime_error` | `publish()`, `createStream()`, `getStreamEntry()` |
| Return `bool`          | `add_peer()`, `remove_peer()`              |
| Log and continue       | `loadKeys()`, `saveAllChunks()`            |
| Silent no-op           | `freeChunk()` on already-freed chunks      |

This inconsistency makes it hard to reason about failure modes. For example,
`saveAllChunks()` logs chunk-save failures but continues saving indexes —
leaving a partially-saved state with no caller notification.

---

## 7. Test Quality

### 7.1 Trivial / empty assertions — HIGH

Multiple test files use `REQUIRE(true)` or `SUCCEED(...)` as the only
assertion, testing nothing beyond "no crash":

| File | Count | Examples |
|------|------:|---------|
| `server_tests.cpp` | 6 | "Server Construction", "SSL context configuration", "Timer armed" all assert `REQUIRE(true)` |
| `rpc_expansion_tests.cpp` | 8 | Constructs JSON objects and asserts field existence; never calls actual RPC handlers |
| `chunk_persistence_tests.cpp` | 2 | "Periodic timer skips save when not dirty" never verifies save was skipped |
| `lifecycle_tests.cpp` | 3 | "saveAllChunks saves only dirty chunks" asserts no-throw, not that only dirty chunks were saved |

### 7.2 Tests that pass vacuously — HIGH

Tests whose pass/fail outcome is independent of the behavior under test:

| File | Test | Issue |
|------|------|-------|
| `block_propagation_tests.cpp` | "Rate limiter allows up to limit then rejects" | Tracks `accepted` count but never verifies blocks 11–12 were *actually dropped* by rate limiting |
| `block_propagation_tests.cpp` | "Pending pool capacity eviction" | Asserts `SUCCEED("...without crash")` — never checks pool size or that oldest was evicted |
| `sync_tests.cpp` | "Difficulty cache invalidated on replaceChain" | Ends with `SUCCEED(...)`, no assertion that cache was actually cleared |
| `consensus_tests.cpp` | "Chain reorg deeper than maxReorgDepth is rejected" | Asserts blockchain still has 1 block, but would pass even if validator ignored the depth check |
| `rpc_expansion_tests.cpp` | All "publish RPC error codes" tests | Build JSON objects and assert fields exist; never invoke actual RPC handler to test error generation |

### 7.3 `rpc_expansion_tests.cpp` tests no actual RPC logic — HIGH

All tests in this file construct JSON response objects manually and assert
their structure. Zero tests invoke `RpcServer::do_read()` or any handler
function. Every test would pass identically if all RPC methods were deleted
from the codebase.

**Recommendation:** Rewrite as integration tests that send JSON-RPC requests
to a running `RpcServer` over a socket (as `rpc_integration_tests.cpp` does),
or extract handler functions from `do_read()` and unit-test them directly.

### 7.4 Integration tests are timing-dependent — MEDIUM

| File | Issue |
|------|-------|
| `p2p_sync_integration_tests.cpp` | `wait_for_chain_length()` polls for 10s with 250ms sleep; `wait_for_outbound_peers()` same |
| `rpc_integration_tests.cpp` | Calls `client->call()` without verifying connection is ready |
| `chunk_persistence_tests.cpp` | `io.run_for(100ms)` may not fire periodic timer on slow machines |

These tests are flaky under CI load. Consider using `io_context::poll()` to
advance deterministically, or condition-variable signaling.

### 7.5 Coverage gaps — MEDIUM

The following behaviors have no test coverage:

| Untested area | Severity |
|---------------|----------|
| `handle_sync_response()` actually appending blocks | High |
| `getBlockByIndex` RPC with out-of-range index | High |
| `--seed-node` CLI with non-numeric port | High |
| Seed node parsing with invalid port values | High |
| Partial `saveAllChunks()` failure (one chunk fails, others continue) | High |
| Chain sync completing end-to-end (blocks actually appended) | High |
| Peer disconnect during propagation | Medium |
| Rate limiter resetting after time window expires | Medium |
| Pending pool TTL-based expiry of stale blocks | Medium |
| Block propagation relay excludes sender correctly | Medium |
| `recoverChain()` with corrupted index files (fallback to chunk rebuild) | Medium |

### 7.6 Duplicated test setup persists in 4 files — LOW

Despite `TestHelpers.hpp` existing, `sync_tests.cpp`,
`block_propagation_tests.cpp`, `consensus_tests.cpp`, and
`chunk_persistence_tests.cpp` still define local `mineTestBlock()` /
`buildValidChain()` / temp directory helpers instead of using the shared
utilities.

---

## 8. Recommendations

Ordered by impact and effort:

| # | Action | Impact | Effort |
|---|--------|--------|--------|
| 1 | Fix `handle_sync_response()` to actually append blocks (§2.1) | Sync is completely broken | Trivial |
| 2 | Add bounds check to `getBlockByIndex` RPC (§3.2) | Prevents crash from RPC input | Trivial |
| 3 | Wrap seed-node port parsing in try/catch with range check (§3.1) | Prevents crash on invalid CLI input | Trivial |
| 4 | Validate port range in `parsePeerKey()` (§2.4) | Prevents silent truncation | Trivial |
| 5 | Rewrite `rpc_expansion_tests.cpp` to test real RPC handlers (§7.3) | False confidence → real coverage | Medium |
| 6 | Replace trivial assertions with meaningful ones (§7.1, §7.2) | Catches actual regressions | Medium |
| 7 | Cache chunk during `recoverChain()` validation (§2.2, §4.3) | 3× faster startup | Low |
| 8 | Replace O(n) peer lookups with `unordered_map` (§4.1) | O(1) peer operations | Medium |
| 9 | Extract RPC dispatch table from `do_read()` (§4.2) | Maintainability, testability | Medium |
| 10 | Narrow `IBlockchain` into reader/writer interfaces (§6.1) | Reduces coupling | Medium |
| 11 | Remove local test helpers in favor of `TestHelpers.hpp` (§7.6) | Consistency | Low |
| 12 | Make integration tests deterministic (§7.4) | Reduces CI flakiness | Medium |
