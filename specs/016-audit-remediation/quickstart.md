# Quickstart: Code Audit Remediation

**Feature**: 016-audit-remediation  
**Date**: 2026-04-12

---

## Prerequisites

- C++20 compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Boost (Asio, Serialization)
- OpenSSL
- Catch2 (test framework)
- GNU Autotools

## Build

```bash
make -j8
```

## Run Tests

Run each test binary individually per constitution §III:

```bash
./tests/blockchain_tests
./tests/lifecycle_tests
./tests/lifecycle_integration_tests
./tests/block_propagation_tests
./tests/block_propagation_integration_tests
./tests/chunk_persistence_tests
./tests/chunk_recovery_tests
./tests/chunk_replace_tests
./tests/consensus_tests
./tests/cli_tests
./tests/merkle_tests
./tests/merkle_rpc_integration_tests
./tests/rpc_expansion_tests
./tests/server_tests
./tests/p2p_sync_integration_tests
```

## Verify Specific Fixes

### Block count correctness (FR-001)
Run `blockchain_tests` — look for tests that build multi-chunk chains and verify `getChainBlockCount()` matches `totalBlockCount_` after freeing chunks.

### Thread safety enforcement (FR-002)
Inspect `src/main.cpp` — the `io_context.run()` call site should have a thread-count assertion and documentation comment.

### Difficulty cache (FR-003)
Run `consensus_tests` — look for tests that verify difficulty calculation doesn't reload chunks for previously-computed boundaries.

### Chunk retention (FR-004)
Run `chunk_persistence_tests` — look for tests that verify chunks remain loaded during multi-access operations.

### Shared utilities (FR-005, FR-006, FR-007, FR-008)
Grep for removed patterns to verify extraction:
```bash
# Should find zero occurrences of inline chunk filename construction:
grep -rn 'setw(6).*\.dat' src/

# Should find zero occurrences of repeated RPC response boilerplate:
grep -c 'response\["jsonrpc"\] = "2.0"' src/network/RpcServer.cpp
# Expected: 2 (only in makeJsonRpcError and makeJsonRpcResult)

# Should find zero occurrences of sender_key.find(':'):
grep -rn "sender_key.find(':')" src/
```

### Pending pool (FR-013)
Run `block_propagation_tests` — look for tests that verify O(1) eviction behavior with the new data structure.

## Key Files Changed

| File | Changes |
|------|---------|
| `src/Blockchain.cpp` | FR-001, FR-003, FR-004, FR-011, FR-012 |
| `src/Blockchain.hpp` | FR-003, FR-004 new members |
| `src/Block.cpp` | FR-009 new constructor |
| `src/Block.hpp` | FR-009 new constructor declaration |
| `src/BlockPropagation.cpp` | FR-008, FR-013 |
| `src/BlockPropagation.hpp` | FR-013 data structure change |
| `src/PeerManager.cpp` | FR-007 unified send_to_peers |
| `src/PeerManager.hpp` | FR-007 method signature change |
| `src/main.cpp` | FR-002 thread assertion |
| `src/utils.cpp` | FR-005, FR-008 new utilities |
| `src/utils.hpp` | FR-005, FR-008 new declarations |
| `src/network/RpcServer.cpp` | FR-006 shared helpers |
| `tests/TestHelpers.hpp` | FR-010 shared test utilities (NEW) |
| `tests/*.cpp` | FR-010 updated to use TestHelpers |
