# Code Audit Report

**Date:** 2026-04-12  
**Scope:** Full codebase — `src/` (5,887 lines) and `tests/` (6,752 lines)

---

## Table of Contents

1. [Executive Summary](#1-executive-summary)
2. [Bugs](#2-bugs)
3. [Code Duplication](#3-code-duplication)
4. [Performance Issues](#4-performance-issues)
5. [Thread Safety](#5-thread-safety)
6. [Architecture Concerns](#6-architecture-concerns)
7. [Test Quality](#7-test-quality)
8. [Recommendations](#8-recommendations)

---

## 1. Executive Summary

The codebase is reasonably well-structured for its size. The templated
`Blockchain<ChunkHandler>` design allows mock injection in tests, and the
chunk-based persistence keeps memory usage bounded. That said, the audit
uncovered **5 bugs**, **6 duplication clusters**, **8 performance issues**,
**thread-safety gaps in the async networking layer**, and **test suite
weaknesses** that could hide regressions.

| Category              | Count | Highest Severity |
|-----------------------|------:|------------------|
| Bugs                  |     5 | High             |
| Code duplication      |     6 | Medium           |
| Performance issues    |     8 | High             |
| Thread-safety gaps    |     4 | High             |
| Test quality issues   |    12 | High             |

---

## 2. Bugs

### 2.1 `getChainBlockCount()` ignores cached `totalBlockCount_` — HIGH

[Blockchain.cpp](../src/Blockchain.cpp#L473-L479)

```cpp
size_t Blockchain<ChunkHandler>::getChainBlockCount() const {
    size_t count = 0;
    for (const auto &chunk : this->chain) {
        count += chunk.blocks.size();   // O(n) every call
    }
    return count;
}
```

`totalBlockCount_` is carefully maintained by `publish()`, `appendBlock()`,
`recoverChain()`, and `replaceChain()`, yet `getChainBlockCount()` always
recomputes by walking every chunk. Because older chunks are freed from memory
(`.clear()`) after use, the walk **under-counts** — cleared chunks report
`blocks.size() == 0`.

This method is called from `on_block_received()`, `calculateNewDifficulty()`,
`getDifficultyForHeight()`, and `replaceChain()`, so the miscount propagates
into consensus validation, difficulty adjustment, and chain-replacement
decisions.

**Fix:** Return `totalBlockCount_` directly, matching `getChainLength()` which
already does this correctly.

### 2.2 Merkle root recomputed on deserialized blocks — MEDIUM

[Block.cpp](../src/Block.cpp#L21-L33)

The parameterised `Block` constructor always recomputes `merkleRoot` from
entries and then recalculates the hash. When blocks are loaded from disk via
Boost.Serialization, the default constructor is used (no recomputation). But
when blocks are constructed on receipt during sync
(`SyncResponse` → `Block(index, time, prev, entries, nonce, diff)`), the
merkle root and hash are recomputed even though the sending node already
provides them. This wastes CPU proportional to the number of entries per block.

### 2.3 `sender_key` parsing duplicated and fragile — MEDIUM

[BlockPropagation.cpp](../src/BlockPropagation.cpp#L122-L127) and line 170

The pattern `sender_key.find(':')` → `substr()` → `std::stoi()` is used twice
in `on_block_received()` to extract host/port from a colon-delimited key

```cpp
auto colon = sender_key.find(':');
if (colon != std::string::npos) {
    auto host = sender_key.substr(0, colon);
    auto port = static_cast<uint16_t>(std::stoi(sender_key.substr(colon + 1)));
    peer_manager_->increment_error(host, port);
}
```

If `sender_key` contains an IPv6 address with colons (e.g. `[::1]:8333`),
`find(':')` splits at the wrong position. This is a latent bug because
`normalize_address()` in `PeerManager` strips `::ffff:` prefixes but does not
bracket raw IPv6 addresses.

### 2.4 `getBlockByIndex()` frees chunk it just loaded — LOW

[Blockchain.cpp](../src/Blockchain.cpp#L287-L298)

```cpp
bool wasEmpty = this->chain.at(chunkIndex).blocks.empty();
if (wasEmpty || !this->chain.at(chunkIndex).isBlockPresent(index % this->chunkSize)) {
    this->loadChunk(chunkIndex);
}
Block result = this->chain.at(chunkIndex).at(index % this->chunkSize);
if (wasEmpty && chunkIndex + 1 < this->chain.size()) {
    this->freeChunk(chunkIndex);      // Immediately frees what was just loaded
}
```

When `getDifficultyForHeight()` calls `getBlockByIndex()` in a loop for each
adjustment boundary, the same chunk is loaded and freed repeatedly.

### 2.5 `dirty_` flag prematurely cleared — LOW

[Blockchain.cpp](../src/Blockchain.cpp#L74)

In `publish()`, when a chunk fills up and a new chunk is created, `dirty_` is
set to `false` before the new block is appended. The subsequent append sets it
back to `true`, but if a crash occurred between the two statements, the
in-memory state would be inconsistent.

---

## 3. Code Duplication

### 3.1 RPC error-response helpers — 9 near-identical methods

[RpcServer.cpp](../src/network/RpcServer.cpp#L770-L847) defines
`invalidJsonRpcMessage()`, `noIdMessage()`, `invalidMethodMessage()`,
`miningTimeoutMessage()`, `syncInProgressMessage()`, `syncStartedMessage()`,
`noPeerMessage()`, `syncAlreadyInProgressMessage()`, `errorMessage()`, and
`errorMessageWithData()`. All repeat `response["jsonrpc"] = "2.0"` and
`response["id"] = id` verbatim.

A single `makeResponse(id, code, message, data = nullptr)` would replace them.

### 3.2 `broadcast_block()` / `relay_block()` — near-identical loops

[PeerManager.cpp](../src/PeerManager.cpp#L636-L672)

The only difference is an `if (key == exclude_key) continue;` guard in
`relay_block()`. Extracting a shared `send_to_peers(block, exclude_key = "")`
helper eliminates ~30 duplicated lines including identical stale-session
cleanup.

### 3.3 Packet serialization in PeerClient and PeerServer

`PeerClient::send<T>()` ([PeerClient.cpp](../src/network/PeerClient.cpp#L352-L375)) and
`PeerServer::send_packet<T>()` ([PeerServer.cpp](../src/network/PeerServer.cpp#L271-L300))
share the same serialize → `PacketHeader` → `memcpy` → `async_write` pattern.
This could live in a shared utility or base class.

### 3.4 `getStreamEntries()` — duplicated block-lookup loop

[Blockchain.cpp](../src/Blockchain.cpp#L155-L197)

The with-key and without-key branches contain a 12-line block-lookup loop that
differs only in the final filter predicate (`e.key == key` vs. always true).
This is a prime candidate for a small lambda extraction.

### 3.5 Chunk filename construction — repeated 6 times

The pattern `"chunk_" + setfill('0') + setw(6) + index + ".dat"` appears in
`publish()`, `appendBlock()`, `discoverChunks()`, `validateChunk()`,
`archiveChainFiles()`, and `recoverChain()`. A `chunkFilename(size_t)` helper
would centralise it.

### 3.6 Test setup boilerplate

Temp directory creation/cleanup, `ConsensusConfig` with `difficulty=0`,
`mineTestBlock()`, and valid-chain building are duplicated across 7–10 test files.
A shared `tests/TestHelpers.hpp` would reduce ~200 lines of redundancy.

---

## 4. Performance Issues

### 4.1 `getChainBlockCount()` walks freed chunks — HIGH

As described in §2.1, this O(n) walk returns incorrect results because cleared
chunks report 0 blocks. Every caller that relies on this count (difficulty
adjustment, chain replacement, block reception) gets a wrong answer.

### 4.2 `getDifficultyForHeight()` — O(W × D) disk I/O — HIGH

[Blockchain.cpp](../src/Blockchain.cpp#L962-L1002)

For height `h` with adjustment window `w`, this method performs `h/w` iterations,
each calling `getBlockByIndex()` twice (which loads and immediately frees
chunks per §2.4). For a chain of 100,000 blocks with window 10, this triggers
20,000 chunk load/free cycles.

**Fix:** Cache the running difficulty per adjustment boundary, or store it in
the block header.

### 4.3 O(n) peer lookups — MEDIUM

`find_peer()`, `add_peer()`, `remove_peer()`, `is_banned()`, and
`get_non_banned_peer_addresses()` all perform linear scans over
`std::vector<PeerEntry>` and `std::vector<BanRecord>`.

With the configured maximum of 256 stored peers and frequent
`is_banned()` calls on every operation, switching to
`std::unordered_map<std::string, PeerEntry>` keyed by `host:port` would make
lookups O(1).

### 4.4 O(n) pending-pool eviction — MEDIUM

[BlockPropagation.cpp](../src/BlockPropagation.cpp#L63-L74)

When the pending pool is full, the oldest entry is found via linear scan.
Using `std::map` ordered by insertion time, or a combined map + deque, would
make eviction O(log n) or O(1).

### 4.5 Chunk loaded 3 times during `recoverChain()` validation — MEDIUM

[Blockchain.cpp](../src/Blockchain.cpp#L590-L620)

In the non-fast-startup path, `validateChunk(i)` loads chunk `i`,
then the cross-chunk check loads chunks `i-1` **and** `i` again.
Each chunk is deserialized from disk up to 3 times.

### 4.6 RPC dispatch is a 21-branch `if`/`else` chain — LOW

[RpcServer.cpp](../src/network/RpcServer.cpp#L74-L704)

Every request walks up to 21 string comparisons. A
`std::unordered_map<std::string, Handler>` dispatch table would be O(1) and
reduce the 630-line monolith into individually testable handler functions.

### 4.7 String construction in log calls — LOW

Throughout the codebase, `logMessage("INFO", "Block #" + std::to_string(...) + ...)`
constructs the string even when the log level would suppress it. A log-level
check before construction (or a lazy-evaluation macro) would eliminate this.

### 4.8 `replaceChain()` loads entire candidate into memory — LOW

[Blockchain.cpp](../src/Blockchain.cpp#L844-L876)

`replaceChain()` accepts `const std::vector<Block> &candidateBlocks` — the
full chain. For very long chains this means the entire history must fit in RAM
simultaneously. A streaming/chunked replacement would bound memory usage.

---

## 5. Thread Safety

The P2P layer runs on `boost::asio::io_context` with potentially multiple
threads (via `io_context.run()` in `main()`). Several data structures are
accessed from async callbacks without synchronization:

| Component             | Shared State                              | Risk                    |
|-----------------------|-------------------------------------------|-------------------------|
| `BlockPropagation`    | `dedup_set_`, `dedup_order_`, `pending_pool_`, `rate_states_`, `sync_queue_` | Concurrent block arrivals from multiple peers corrupt containers |
| `PeerManager`         | `peers_`, `bans_`, `backoff_state_`, `inbound_sessions_`, `outbound_connections_` | Peer exchange + disconnect callbacks race |
| `Blockchain`          | `dirty_`, `totalBlockCount_`, `chain`, `streamKeyIndex` | Periodic save timer races with `appendBlock()` |
| `RpcServer`           | `buffer` (boost::asio streambuf)          | Concurrent RPC requests on same connection |

**If `io_context` is run on a single thread**, these are safe due to
implicit strand serialisation. However, nothing in the codebase enforces
single-threaded execution, and `main.cpp` does not document this constraint.

**Recommendation:** Either enforce single-threaded `io_context::run()` with a
comment, or add `boost::asio::strand` wrappers around shared state and
explicitly document the threading model.

---

## 6. Architecture Concerns

### 6.1 `RpcServer.cpp` is an 847-line monolith

All 21 RPC methods live in a single `do_read()` callback. This makes the file
hard to navigate, difficult to unit-test individual handlers, and prone to
merge conflicts.

**Suggestion:** Extract each RPC method into a standalone handler function
(or a `std::unordered_map<std::string, std::function<json(json)>>` dispatch
table). This reduces `do_read()` to ~30 lines and makes each handler
independently testable.

### 6.2 `Blockchain.cpp` at 1,024 lines mixes concerns

`Blockchain.cpp` handles block creation/mining, persistence (save/load chunks,
keys, streams, stream index), chain recovery, validation, difficulty
adjustment, chain replacement, archiving, periodic save timers, Merkle proofs,
and stream queries.

**Suggestion:** Split into focused modules:
- `ChainPersistence` — save/load/recover/archive chunks and indexes
- `DifficultyEngine` — difficulty calculation and adjustment
- `MerkleProofService` — proof generation and verification  
- `Blockchain` — core chain operations (publish, append, replace)

### 6.3 No separation between domain and network layers

`PeerClient`, `PeerServer`, and `BlockPropagation` directly call
`IBlockchain` methods. If the consensus rules or block format change, the
network layer must change too.

**Suggestion:** Introduce a thin service layer (e.g. `ChainService`) that
mediates between the network and domain layers. The network layer would submit
blocks to the service, which validates and delegates to `Blockchain`.

### 6.4 `IBlockchain` interface is wide

[IBlockchain.hpp](../src/IBlockchain.hpp) exposes 30+ methods including
persistence (`saveChunk`, `saveKeys`), mining (`publish`), querying
(`getStreamEntries`), and sync (`replaceChain`). Consumers that only need read
access (e.g. `RpcServer` for query endpoints) are coupled to the full
interface.

**Suggestion:** Split into `IChainReader` (query methods) and `IChainWriter`
(mutation methods). `RpcServer` depends only on `IChainReader` plus a small
`IChainWriter` for `publish` and `createStream`.

### 6.5 Error handling is inconsistent

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

### 7.1 Trivial / empty assertions

| File | Test | Issue |
|------|------|-------|
| `server_tests.cpp` | "Server Construction" | Asserts `REQUIRE(true)` — meaningless |
| `chunk_persistence_tests.cpp` | "Periodic timer skips save when not dirty" | Never verifies save was skipped |
| `rpc_expansion_tests.cpp` | Multiple handlers | Manually constructs JSON instead of calling actual RPC handler |

### 7.2 Tests that pass vacuously

`block_propagation_tests.cpp` — "Duplicate block discarded silently" never
calls `appendBlock()`, so the relay callback is never invoked. The test passes
because the `relay_count` variable stays at 0 regardless of whether dedup
works.

### 7.3 Coverage gaps

| Untested area | Severity |
|---------------|----------|
| SSL/TLS handshake rejection | High |
| Peer timeout during sync | High |
| `publish()` to unauthorized stream via RPC | High |
| CLI with invalid values (`--rpc-port abc`) | High |
| Partial `saveAllChunks()` failure | High |
| Chain reorg from actual P2P sync | High |
| Concurrent block propagation (multi-peer) | Medium |
| Difficulty increase after target achievement | Medium |
| Ban expiration by timer | Medium |
| Merkle proof with 1000+ entries | Medium |

### 7.4 Duplicated test setup

Temp-directory helpers, `mineTestBlock()`, `ConsensusConfig{difficulty=0}`,
and valid-chain building appear identically in 7–10 files. A shared
`tests/TestHelpers.hpp` would eliminate ~200 lines and ensure
consistent setup.

### 7.5 Integration tests are timing-dependent

`p2p_sync_integration_tests.cpp` uses a hardcoded 10-second `wait_for_chain_length()`
loop. Under CI load this can flake. Consider using condition variables or
`io_context::poll()` to advance deterministically.

---

## 8. Recommendations

Ordered by impact and effort:

| # | Action | Impact | Effort |
|---|--------|--------|--------|
| 1 | Fix `getChainBlockCount()` to return `totalBlockCount_` | Correctness bug (§2.1, §4.1) | Trivial |
| 2 | Add mutex/strand or enforce single-thread contract (§5) | Prevents data corruption | Low |
| 3 | Cache difficulty per adjustment boundary (§4.2) | Eliminates O(W×D) disk I/O | Medium |
| 4 | Extract shared `TestHelpers.hpp` (§7.4) | Reduces 200 lines of test duplication | Low |
| 5 | Replace RPC `if`/`else` chain with dispatch table (§4.6, §6.1) | Maintainability, testability | Medium |
| 6 | Centralise chunk-filename and peer-key helpers (§3.5, §3.2) | Removes 6 duplication sites | Low |
| 7 | Fix `sender_key` IPv6 parsing (§2.3) | Prevents misparse on IPv6 networks | Low |
| 8 | Add `chunkFilename()` utility (§3.5) | Single source of truth for paths | Trivial |
| 9 | Split `Blockchain.cpp` into focused modules (§6.2) | Maintainability at scale | High |
| 10 | Narrow `IBlockchain` into reader/writer interfaces (§6.4) | Reduces coupling | Medium |
