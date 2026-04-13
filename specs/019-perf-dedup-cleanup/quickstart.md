# Quickstart: Performance & Deduplication Cleanup

## What This Feature Changes

Five internal refactors — no user-facing protocol or API changes:

1. **Peer lookups** become O(1) instead of O(n)
2. **RPC dispatch** becomes O(1) instead of O(n) and handlers are individually testable
3. **Packet serialization** is deduplicated between PeerClient and PeerServer
4. **Test helpers** are consolidated into TestHelpers.hpp
5. **Log formatting** is skipped for suppressed messages

## Build & Test

```bash
# Build (constitution requires -j8)
make -j8

# Run test binaries individually (constitution requirement)
./tests/blockchain_tests
./tests/block_propagation_tests
./tests/chunk_persistence_tests
./tests/lifecycle_tests
./tests/rpc_integration_tests
./tests/p2p_sync_integration_tests
```

## Key Files to Review

| File | Change Type | What Changed |
|------|------------|--------------|
| `src/PeerManager.hpp` | Modified | `peers_` and `bans_` container types |
| `src/PeerManager.cpp` | Modified | All peer/ban CRUD operations |
| `src/network/RpcServer.hpp` | Modified | Dispatch table type declaration |
| `src/network/RpcServer.cpp` | Modified | Handlers extracted, dispatch table |
| `src/network/PacketSerializer.hpp` | **New** | Shared serialization template |
| `src/network/PeerClient.cpp` | Modified | Uses PacketSerializer |
| `src/network/PeerServer.cpp` | Modified | Uses PacketSerializer |
| `src/utils.hpp` | Modified | LOG_MSG / LOG_INFO / etc. macros |
| `tests/TestHelpers.hpp` | Modified | Added `make_block()` helper |
| `tests/sync_tests.cpp` | Modified | Removed local helpers |
| `tests/consensus_tests.cpp` | Modified | Removed local helpers |
| `tests/block_propagation_tests.cpp` | Modified | Removed local helpers |
| `tests/chunk_persistence_tests.cpp` | Modified | Removed local helpers |

## Verification Checklist

- [ ] All 20 RPC methods return identical responses (run `rpc_integration_tests`)
- [ ] P2P sync and block propagation work (run `p2p_sync_integration_tests`)
- [ ] No local `mineTestBlock` / `buildValidChain` / `make_block` definitions outside TestHelpers.hpp
- [ ] `peers.json` loads correctly from existing files (no migration needed)
- [ ] `LOG_INFO(...)` macro compiles and works on Linux, macOS, Windows
