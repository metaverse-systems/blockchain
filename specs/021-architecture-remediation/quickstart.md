# Quickstart: Architecture Remediation

**Feature**: 021-architecture-remediation  
**Branch**: `021-architecture-remediation`

## What This Feature Does

Refactors the blockchain architecture to address three audit concerns and one performance issue:

1. **Narrows interfaces** — Read-only consumers (RpcServer, PeerServer for queries) depend on `IChainReader`, not the full `IBlockchain`. Write operations compile-time-gated to only the methods needed.
2. **Adds a service layer** — `ChainService` mediates between network components and the blockchain domain. Network code submits blocks; the service handles validation, append, and persistence.
3. **Standardizes error handling** — All operations use domain-specific exceptions (`ChainError` hierarchy). No more silent failures, boolean returns for errors, or log-and-continue patterns.
4. **Streaming chain replacement** — `replaceChain()` processes candidates in 100-block batches, bounding peak memory. Original chain preserved on validation failure.

## Files Changed

### New Files
- `src/ChainService.hpp` / `src/ChainService.cpp` — Service layer
- `src/ChainError.hpp` — Exception hierarchy
- `tests/chain_service_tests.cpp` — Service layer unit tests

### Modified Files (Interface Changes)
- `src/IChainReader.hpp` — Add 4 query methods from IBlockchain
- `src/IChainWriter.hpp` — Exception contract documentation
- `src/IBlockchain.hpp` — Remove 4 query methods (moved to IChainReader), retain persistence methods
- `src/network/SyncMessages.hpp` — Replace `chunk_index` with `start_index`

### Modified Files (Consumer Changes)
- `src/network/RpcServer.hpp` / `.cpp` — Depend on `IChainReader` + `IChainWriter` instead of `IBlockchain`
- `src/network/PeerServer.hpp` / `.cpp` — Use `ChainService` for sync reads, remove chunkSize references
- `src/network/PeerClient.hpp` / `.cpp` — Use `ChainService::submitSyncBatch()` instead of direct append/save
- `src/BlockPropagation.hpp` / `.cpp` — Use `ChainService::submitBlock()` instead of direct append/save
- `src/network/SessionHandler.hpp` — Template parameter for narrower interface types

### Modified Files (Error Handling)
- `src/Blockchain.cpp` — Use `ChainError` subclasses, streaming replaceChain
- `src/ChainPersistence.cpp` — Throw `PersistenceError` instead of log-and-continue
- `src/PeerManager.hpp` / `.cpp` — Throw `PeerError` instead of returning bool

### Modified Files (Build/Config)
- `src/Makefile.am` — Add ChainService.cpp, ChainError.hpp
- `tests/Makefile.am` — Add chain_service_tests
- `.gitignore` — Add new test binary
- `docs/ROADMAP.md` — Mark feature complete
- `main.cpp` — Wire ChainService into component graph

## Build & Test

```bash
make -j8
./tests/blockchain_tests
./tests/chain_service_tests
./tests/lifecycle_tests
./tests/lifecycle_integration_tests
./tests/rpc_expansion_tests
./tests/rpc_integration_tests
./tests/p2p_sync_integration_tests
./tests/block_propagation_tests
./tests/block_propagation_integration_tests
./tests/chunk_persistence_tests
./tests/chunk_replace_tests
./tests/sync_tests
./tests/consensus_tests
```

## Key Design Decisions

- `ChainService` is a **thin mediator**, not a full domain service. It delegates all validation to `IBlockchain::isValidNewBlock()`.
- `RpcServer` takes **two narrow interfaces** (`const IChainReader&` + `IChainWriter&`) instead of `IBlockchain&`.
- Exception hierarchy extends `std::runtime_error` so existing `catch(std::runtime_error&)` patterns continue to work during migration.
- Streaming `replaceChain` uses temporary files and atomic rename for crash safety.
- Wire format change (remove `chunk_index`, add `start_index`) is a breaking change — all nodes update together.
