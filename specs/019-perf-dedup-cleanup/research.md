# Research: Performance & Deduplication Cleanup

**Date**: 2026-04-13  
**Feature**: 019-perf-dedup-cleanup  

## R1: Peer Container Migration (vector → unordered_map)

### Decision
Replace `std::vector<PeerEntry> peers_` with `std::unordered_map<std::string, PeerEntry>` keyed by `peer_key(host, port)`. Replace `std::vector<BanRecord> bans_` with `std::unordered_map<std::string, BanRecord>` keyed by the same key format.

### Rationale
- `peer_key()` already exists as a `static` helper in PeerManager — it constructs `"host:port"` strings. The infrastructure for consistent key generation is already in place.
- `find_peer()`, `add_peer()`, `remove_peer()`, `is_banned()` all do linear scans with the same key predicate (`host == x && port == y`). Switching to map keyed by `peer_key()` makes all these O(1).
- `get_peers()` returns `std::vector<PeerEntry>` — this public API can remain unchanged by iterating the map values.
- `get_bans()` returns `std::vector<BanRecord>` — same approach.

### Alternatives Considered
1. **`std::map<std::string, PeerEntry>`**: O(log n) instead of O(1). No advantage at n <= 256 since hash is faster for string keys.
2. **Custom hash on PeerEntry**: Over-engineered; string key from existing `peer_key()` is simple and works.
3. **Keep vector + add index**: Dual data structure maintenance with no benefit.

### Migration Impact

| Method | Current Pattern | New Pattern |
|--------|----------------|-------------|
| `find_peer()` | Linear scan | `peers_.find(key)` → iterator |
| `add_peer()` | Linear scan + push_back | `peers_[key]` update or insert |
| `remove_peer()` | `std::remove_if` + erase | `peers_.erase(key)` |
| `is_banned()` | Linear scan of bans_ | `bans_.find(key)` + expiry check |
| `evict_oldest_peer()` | `std::min_element` | `std::min_element` over map values (unchanged algorithmic pattern) |
| `get_peers()` | Return copy of vector | Iterate map values into vector |
| `get_non_banned_peer_addresses()` | Iterate vector | Iterate map values |
| `save_peers()` | `j["peers"] = peers_` | Serialize map values as JSON array |
| `load_peers()` | `peers_ = j["peers"].get<vector>()` | Deserialize JSON array into map entries |

### JSON Backward Compatibility
`peers.json` stores peers as a JSON array. The on-disk format does not change — `save_peers()` will serialize map values as a JSON array, and `load_peers()` will read the array and insert each entry into the map. Existing `peers.json` files load without migration.

### Pointer Stability
`find_peer()` currently returns `PeerEntry*` — a pointer into the vector. With `std::unordered_map`, pointers/references to values remain stable across insertions (unlike vector). This is an improvement, not a regression. However, `erase()` invalidates the erased element's pointer, which is the expected behavior.

---

## R2: RPC Dispatch Table

### Decision
Replace the 20-branch if/else chain in `do_read()` with an `std::unordered_map<std::string, std::function<nlohmann::json(...)>>` dispatch table initialized once on construction. Each handler is a private member function that receives the parsed JSON request and returns a JSON response.

### Rationale
- 20 methods × string comparison per request = O(n) dispatch. Hash table gives O(1).
- The current `do_read()` is ~700 lines. Extracting handlers into separate functions makes each independently readable and testable.
- The fallback `invalidMethodMessage()` already exists and returns -32601.

### Handler Signature
```
using RpcHandler = std::function<nlohmann::json(const nlohmann::json &request)>;
std::unordered_map<std::string, RpcHandler> dispatch_;
```

Each handler receives the full JSON-RPC request object and returns a complete JSON response (success or error). Per clarification, the dispatcher does not catch exceptions — handlers own their error responses.

### Alternatives Considered
1. **`std::map`**: O(log n) — no advantage over hash map for string keys.
2. **Compile-time dispatch (constexpr map)**: Not available in C++20 for runtime strings in a standard way. Over-engineered.
3. **Switch on hash of method name**: Fragile, hash collisions, harder to maintain.

### Dispatch Loop Pattern
```
auto it = dispatch_.find(method);
if (it != dispatch_.end()) {
    response = it->second(object);
} else {
    response = invalidMethodMessage(object["id"], method);
}
```

### Static Helpers
The existing static helper functions (`errorMessage`, `resultMessage`, `resultJsonMessage`, etc.) remain static — handlers call them to construct responses. No change to helper signatures.

---

## R3: Shared Packet Serialization

### Decision
Create `src/network/PacketSerializer.hpp` — a header-only free function template that encapsulates the serialize → PacketHeader → async_write pattern.

### Rationale
- `PeerClient::send<T>()` and `PeerServer::send_packet<T>()` are nearly identical templates with minor differences in buffer management (PeerClient uses `write_buffer` member; PeerServer uses shared_ptr buffers for lifetime).
- A header-only template avoids needing explicit instantiations in a separate .cpp file.
- The two callers have different socket types (`ssl::stream<tcp::socket>` vs `boost::asio::streambuf`-based) and different lifetime patterns (PeerServer captures `shared_from_this()`). The shared utility must be parameterized on the write target.

### Design
The shared function accepts a pre-serialized buffer (string) + packet type + a write callback, leaving socket/lifetime management to each caller. This is the minimal shared surface:

```cpp
template<typename T>
std::pair<std::vector<char>, std::string> serialize_packet(const T &obj, uint64_t packet_type);
```

Returns the header bytes and serialized payload. Each caller handles `async_write` with its own socket and lifetime management, since PeerClient and PeerServer have fundamentally different ownership models.

### Alternatives Considered
1. **Full async_write in shared function**: Would require abstracting over socket types and lifetime (shared_ptr capture). Too much coupling.
2. **Base class with virtual send**: Runtime overhead for a template function. Not idiomatic C++.
3. **Add to PacketHeader.hpp**: PacketHeader.hpp is a POD struct header — adding Boost.Serialization includes would pollute it.

---

## R4: Lazy Log Formatting

### Decision
Add a preprocessor macro `LOG_MSG(level, expr)` that checks the current log level before evaluating the message expression. The existing `logMessage()` function remains unchanged for backward compatibility.

### Rationale
- `logMessage()` takes `const std::string &msg` — the string is constructed at the call site before the function can check the level.
- A macro can short-circuit: `if (getLogLevel() <= LogLevel::X) logMessage("X", expr)`.
- The `getLogLevel()` function and `LogLevel` enum already exist.

### Design
```cpp
#define LOG_MSG(level_str, level_enum, msg_expr) \
    do { if (static_cast<int>(level_enum) >= static_cast<int>(getLogLevel())) \
        logMessage(level_str, msg_expr); } while(0)

#define LOG_DEBUG(msg) LOG_MSG("DEBUG", LogLevel::Debug, msg)
#define LOG_INFO(msg) LOG_MSG("INFO", LogLevel::Info, msg)
#define LOG_WARN(msg) LOG_MSG("WARN", LogLevel::Warning, msg)
#define LOG_ERROR(msg) LOG_MSG("ERROR", LogLevel::Error, msg)
```

### Alternatives Considered
1. **Template with lambda**: `logLazy(LogLevel::Info, [&]{ return "msg " + x; })` — cleaner but requires changing every call site to lambda syntax. Higher migration cost.
2. **Variadic template with fmt-style**: Would require adding fmt or a format library. Violates constitution principle V (minimal dependencies).
3. **Do nothing**: Acceptable at current scale but inconsistent with constitution principle X (low-latency performance).

### Migration Strategy
New code uses `LOG_INFO(...)` macros. Existing `logMessage()` calls are migrated incrementally — not all in this feature, only the hot-path calls identified in the audit (block propagation, peer exchange, sync). The macro and function coexist indefinitely.

---

## R5: Test Helper Consolidation

### Decision
Extend `TestHelpers.hpp` with two additional helpers, then remove local definitions from the four test files.

### Rationale
Comparison of local helpers vs TestHelpers.hpp:

| Local Helper | File | Key Difference from TestHelpers |
|-------------|------|--------------------------------|
| `mineTestBlock()` | sync_tests.cpp | No merkle root; `difficulty=1` default |
| `mineBlock()` | consensus_tests.cpp | Same as sync_tests but different name |
| `make_block()` | chunk_persistence_tests.cpp | Hardcoded `difficulty=0`, no PoW loop, uses `std::time()` for timestamp |
| `buildValidChain()` | sync_tests.cpp | `difficulty=1` default |

### New Helpers to Add
1. `make_block(index, prevHash)` — fast block creation with `difficulty=0` and no PoW mining, matching `chunk_persistence_tests.cpp` usage. Uses `static_cast<uint64_t>(std::time(nullptr))` for timestamp.
2. No other new helpers needed — the existing `mineTestBlock()` covers `sync_tests.cpp` and `consensus_tests.cpp` usage, and `buildValidChain()` already accepts a difficulty parameter.

### Key Differences to Resolve
- **Merkle root**: TestHelpers `mineTestBlock()` computes merkle root; local versions do not. The test assertions don't check merkle roots (they test sync/consensus behavior). The shared version with merkle root is strictly more correct — tests pass with it.
- **Default difficulty**: sync_tests uses `difficulty=1`; TestHelpers `buildValidChain` uses `difficulty=0`. The sync_tests callers explicitly pass difficulty, so the default doesn't matter.
- **Timestamp**: chunk_persistence uses `std::time(nullptr)`; others use `index * 10`. The `make_block()` helper preserves the `time(nullptr)` pattern for chunk tests.

### Migration Plan
1. Add `make_block(index, prevHash)` to `TestHelpers` namespace.
2. In each test file: remove local helper definitions, add `#include "TestHelpers.hpp"`, replace calls (`mineBlock(...)` → `TestHelpers::mineTestBlock(...)`, `make_block(...)` → `TestHelpers::make_block(...)`).
3. Verify all tests pass.
