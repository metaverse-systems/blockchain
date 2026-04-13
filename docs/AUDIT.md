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

All 4 bugs, both security issues, and 1 performance issue were resolved in
**018-audit-bug-security-fixes**. Remaining items are tracked below.

| Category              | Found | Resolved | Remaining | Highest Open Severity |
|-----------------------|------:|---------:|----------:|----------------------:|
| Bugs                  |     4 |        4 |         0 | —                     |
| Security issues       |     2 |        2 |         0 | —                     |
| Performance issues    |     5 |        1 |         4 | Medium                |
| Code duplication      |     2 |        0 |         2 | Medium                |
| Architecture concerns |     3 |        0 |         3 | Medium                |
| Test quality issues   |     6 |        0 |         6 | High                  |

---

## 2. Bugs

### 2.1 ~~Sync never appends received blocks — HIGH~~ ✅ RESOLVED (018)

`handle_sync_response()` now appends new blocks via `bc.appendBlock()`, checks
overlap hash mismatches (aborting sync on fork detection), calls `bc.saveKeys()`
after chunk saves, and warns when an empty batch arrives with height mismatch.

### 2.2 ~~`recoverChain()` loads each chunk up to 3 times — MEDIUM~~ ✅ RESOLVED (018)

`validateChunk()` now returns `std::optional<ChunkHandler>` and `recoverChain()`
uses a single-pass loop with `prevChunk` caching — each chunk is deserialized
exactly once.

### 2.3 ~~`getBlockByIndex()` passes wrong index to `ChunkHandler` resize — LOW~~ ✅ RESOLVED (018)

Replaced `resize(chunkIndex + 1, ChunkHandler(chunkIndex + 1, ...))` with a
`while (chain.size() <= chunkIndex)` / `emplace_back(chain.size(), ...)` loop,
matching the pattern in `appendBlock()`.

### 2.4 ~~`parsePeerKey()` does not validate port range — LOW~~ ✅ RESOLVED (018)

`parsePeerKey()` now wraps `std::stoi()` in try/catch and validates the port
is in [1, 65535], throwing `std::invalid_argument` with descriptive messages
for non-numeric, out-of-range, and malformed inputs.

---

## 3. Security

### 3.1 ~~Seed node port parsing crashes on invalid input — HIGH~~ ✅ RESOLVED (018)

Seed node parsing in `main.cpp` now uses `parsePeerKey()` wrapped in
try/catch. Invalid input produces a descriptive stderr message and returns
exit code 1.

### 3.2 ~~`getBlockByIndex` RPC has no bounds check — MEDIUM~~ ✅ RESOLVED (018)

The `getBlockByIndex` handler now checks `index >= bc.getChainLength()` and
returns JSON-RPC error -32001 "Block not found" for out-of-range indices.

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

### 4.3 ~~`recoverChain()` loads each chunk multiple times — MEDIUM~~ ✅ RESOLVED (018)

Resolved together with §2.2. Single-pass recovery loads each chunk once.

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

| Untested area | Severity | Status |
|---------------|----------|--------|
| `handle_sync_response()` actually appending blocks | High | ✅ Covered (018) |
| `getBlockByIndex` RPC with out-of-range index | High | ✅ Covered (018) |
| `--seed-node` CLI with non-numeric port | High | ✅ Covered (018) |
| Seed node parsing with invalid port values | High | ✅ Covered (018) |
| Partial `saveAllChunks()` failure (one chunk fails, others continue) | High | Open |
| Chain sync completing end-to-end (blocks actually appended) | High | ✅ Covered (018) |
| Peer disconnect during propagation | Medium | Open |
| Rate limiter resetting after time window expires | Medium | Open |
| Pending pool TTL-based expiry of stale blocks | Medium | Open |
| Block propagation relay excludes sender correctly | Medium | Open |
| `recoverChain()` with corrupted index files (fallback to chunk rebuild) | Medium | Open |

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
| # | Action | Impact | Effort | Status |
|---|--------|--------|--------|--------|
| 1 | Fix `handle_sync_response()` to actually append blocks (§2.1) | Sync is completely broken | Trivial | ✅ Done (018) |
| 2 | Add bounds check to `getBlockByIndex` RPC (§3.2) | Prevents crash from RPC input | Trivial | ✅ Done (018) |
| 3 | Wrap seed-node port parsing in try/catch with range check (§3.1) | Prevents crash on invalid CLI input | Trivial | ✅ Done (018) |
| 4 | Validate port range in `parsePeerKey()` (§2.4) | Prevents silent truncation | Trivial | ✅ Done (018) |
| 5 | Rewrite `rpc_expansion_tests.cpp` to test real RPC handlers (§7.3) | False confidence → real coverage | Medium | Open |
| 6 | Replace trivial assertions with meaningful ones (§7.1, §7.2) | Catches actual regressions | Medium | Open |
| 7 | Cache chunk during `recoverChain()` validation (§2.2, §4.3) | 3× faster startup | Low | ✅ Done (018) |
| 8 | Replace O(n) peer lookups with `unordered_map` (§4.1) | O(1) peer operations | Medium | Open |
| 9 | Extract RPC dispatch table from `do_read()` (§4.2) | Maintainability, testability | Medium | Open |
| 10 | Narrow `IBlockchain` into reader/writer interfaces (§6.1) | Reduces coupling | Medium | Open |
| 11 | Remove local test helpers in favor of `TestHelpers.hpp` (§7.6) | Consistency | Low | Open |
| 12 | Make integration tests deterministic (§7.4) | Reduces CI flakiness | Medium | Open |
