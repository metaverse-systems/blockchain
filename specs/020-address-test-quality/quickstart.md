# Quickstart: Address Test Quality

**Feature**: 020-address-test-quality  
**Branch**: `020-address-test-quality`

## What This Feature Does

Brings the entire test suite to production quality by:
1. Replacing all trivial/vacuous assertions across 26 test files with meaningful behavioral checks
2. Rewriting RPC expansion tests to exercise real handler code
3. Making integration tests deterministic (no timing-dependent flakiness)
4. Adding test coverage for 6 previously untested behaviors
5. Splitting `IBlockchain` into `IChainReader`/`IChainWriter` for narrower mocking
6. Fixing `saveAllChunks()` to report partial failures

## Build & Test

```bash
# Build everything
make -j8

# Run individual test binaries
./tests/blockchain_tests
./tests/lifecycle_tests
./tests/lifecycle_integration_tests
./tests/server_tests
./tests/rpc_expansion_tests
./tests/block_propagation_tests
./tests/chunk_persistence_tests
./tests/consensus_tests
./tests/sync_tests
./tests/p2p_sync_integration_tests
./tests/rpc_integration_tests
# ... (run each test binary individually per constitution §III)
```

## Verify Determinism

```bash
# Run integration tests 10 times to verify no flakiness
for i in {1..10}; do
    echo "Run $i:"
    ./tests/p2p_sync_integration_tests && \
    ./tests/rpc_integration_tests && \
    ./tests/lifecycle_integration_tests && \
    echo "PASS" || { echo "FAIL on run $i"; break; }
done
```

## Verify No Trivial Assertions Remain

```bash
# Should return zero matches for sole-assertion patterns
grep -rn 'REQUIRE(true)' tests/ | grep -v '//'  
grep -rn 'SUCCEED(' tests/ | grep -v '//'
```

## Key Files Changed

| File | Change |
|------|--------|
| `src/IChainReader.hpp` | New read-only interface |
| `src/IChainWriter.hpp` | New mutation interface |
| `src/IBlockchain.hpp` | Now inherits IChainReader + IChainWriter |
| `src/ChainPersistence.hpp` | `saveAllChunks()` returns failure count |
| `src/network/RpcServer.hpp` | Added `friend class RpcHandlerTests` |
| `tests/rpc_expansion_tests.cpp` | Rewritten to call real handlers |
| `tests/*.cpp` (all 26) | Trivial assertions replaced |
