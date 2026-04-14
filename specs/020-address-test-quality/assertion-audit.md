# Assertion Audit Inventory

Systematic audit of all 26 test files for trivial/vacuous assertions.

## Flagged Test Cases

| # | File | Line | Test Name | Pattern | Sole? | Proposed Replacement |
|---|------|------|-----------|---------|-------|----------------------|
| 1 | server_tests.cpp | 51 | Server Construction | `REQUIRE(true)` | Yes | Assert server objects have valid state (e.g., acceptor is open) |
| 2 | server_tests.cpp | 130 | P2P mutual TLS context rejects missing peer cert | `REQUIRE(true)` | Yes | Assert verify mode was actually set via `mutual_ctx.verify_mode()` |
| 3 | server_tests.cpp | 229 | (within sync query test) | `REQUIRE(true)` | No | Assert `getBlocksByKeys` returns expected empty vector |
| 4 | block_propagation_tests.cpp | 49 | RecentBlockCache FIFO eviction at capacity | `SUCCEED(...)` | Yes | Actually fill cache and assert dedup after eviction (size/oldest entry) |
| 5 | block_propagation_tests.cpp | 327 | Pending pool capacity eviction | `SUCCEED(...)` | Yes | Assert pool size == 64 after 65 inserts, assert oldest entry evicted |
| 6 | consensus_tests.cpp | 560 | Difficulty cache invalidated | `SUCCEED(...)` | Yes | Assert `diff_before != diff_after` (different chain → different difficulty) |
| 7 | chunk_persistence_tests.cpp | 332 | ChunkRetainGuard RAII cleanup | `SUCCEED(...)` | No | Assert chunk was freed after guard destruction (e.g., re-load needed) |
| 8 | lifecycle_tests.cpp | 50 | saveAllChunks saves dirty chunks | `REQUIRE_NOTHROW(...)` | No | Assert return value == 0, assert dirty flag cleared |
| 9 | lifecycle_tests.cpp | 61 | saveAllChunks skips empty chunks | `REQUIRE_NOTHROW(...)` | Yes | Assert return value == 0, assert chunk count unchanged |
| 10 | lifecycle_tests.cpp | 305 | Periodic save only writes dirty chunks | `REQUIRE_NOTHROW(...)` | No | Assert return value == 0 |
| 11 | chunk_replace_tests.cpp | 105 | replaceChain handles backup dir creation failure | `REQUIRE_NOTHROW(...)` | Yes | Assert chain state unchanged after failed backup |
| 12 | cli_tests.cpp | 324 | NodeConfig validation valid config passes | `REQUIRE_NOTHROW(...)` | Yes | Assert config fields have expected default values after validate() |
| 13 | cli_tests.cpp | 338 | NodeConfig unknown key warns but does not fail | `REQUIRE_NOTHROW(...)` | Yes | Assert loaded config has the known fields set, unknown ignored |

## Summary

- **Total flagged**: 13 instances across 6 files
- **Sole-assertion cases**: 9 (highest priority)
- **Non-sole but weak**: 4 (secondary priority)
- **Files with no issues**: 20 of 26 test files are clean

## Files Audited (26 total)

- [x] blockchain_tests.cpp — clean
- [x] block_propagation_tests.cpp — 2 issues (#4, #5)
- [x] block_propagation_integration_tests.cpp — clean
- [x] chunk_persistence_tests.cpp — 1 issue (#7)
- [x] chunk_recovery_tests.cpp — clean
- [x] chunk_replace_tests.cpp — 1 issue (#11)
- [x] consensus_tests.cpp — 1 issue (#6)
- [x] cli_tests.cpp — 2 issues (#12, #13)
- [x] difficulty_engine_tests.cpp — clean
- [x] lifecycle_tests.cpp — 3 issues (#8, #9, #10)
- [x] lifecycle_integration_tests.cpp — clean
- [x] merkle_tests.cpp — clean
- [x] merkle_proof_tests.cpp — clean
- [x] merkle_rpc_integration_tests.cpp — clean
- [x] p2p_sync_integration_tests.cpp — clean
- [x] rpc_expansion_tests.cpp — clean
- [x] rpc_integration_tests.cpp — clean
- [x] server_tests.cpp — 3 issues (#1, #2, #3)
- [x] chain_persistence_module_tests.cpp — clean
