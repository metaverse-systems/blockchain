# Data Model: Compile-Time Optimization

**Feature**: 015-compile-time-optimization  
**Date**: 2026-04-12

## Entities

This feature modifies build system configuration files, not runtime data structures. The "entities" are the build artifacts and their relationships.

### Static Archive: `libblockchain_core.a`

| Attribute | Value |
|-----------|-------|
| Location | `src/` (built by `src/Makefile.am`) |
| Type | `noinst_LIBRARIES` (internal, not installed) |
| Sources | Block.cpp, Blockchain.cpp, Chunk.cpp, utils.cpp, NodeConfig.cpp, PeerManager.cpp, BlockPropagation.cpp, MerkleTree.cpp, network/RpcServer.cpp, network/PeerServer.cpp, network/PeerClient.cpp |
| Flags | `-std=c++20 -Wall -Wextra -pedantic $(BOOST_CPPFLAGS) ${OPENSSL_CFLAGS}` |
| Consumers | `blockchain` (main), all 13 `check_PROGRAMS` |

### Main Binary: `blockchain`

| Attribute | Value |
|-----------|-------|
| Location | `src/` |
| Own sources | `main.cpp`, `CliParser.cpp` |
| Links | `libblockchain_core.a` + OpenSSL + Boost.Serialization + Boost.ProgramOptions + PLATFORM_LIBS |
| Own flags | `-std=c++20 -Wall -Wextra -pedantic -O3 ${OPENSSL_CFLAGS} $(BOOST_CPPFLAGS)` (for main.cpp, CliParser.cpp only) |

### Test Binaries (13 targets)

| Attribute | Value |
|-----------|-------|
| Location | `tests/` |
| Own sources | Each has 1+ test `.cpp` file(s) |
| Links | `../src/libblockchain_core.a` + OpenSSL + Boost.Serialization + Catch2 + PLATFORM_LIBS |
| Exception | `cli_tests` also links Boost.ProgramOptions and compiles `../src/CliParser.cpp` |

## Relationships

```
libblockchain_core.a (11 source files, compiled once)
├── blockchain (links archive + main.cpp + CliParser.cpp)
├── blockchain_tests (links archive + 9 test .cpp files)
├── block_propagation_tests (links archive + 1 test .cpp)
├── block_propagation_integration_tests (links archive + 1 test .cpp)
├── chunk_persistence_tests (links archive + 1 test .cpp)
├── chunk_recovery_tests (links archive + 1 test .cpp)
├── chunk_replace_tests (links archive + 1 test .cpp)
├── merkle_tests (links archive + 1 test .cpp)
├── merkle_rpc_integration_tests (links archive + 1 test .cpp)
├── cli_tests (links archive + CliParser.cpp + 1 test .cpp)
├── rpc_expansion_tests (links archive + 1 test .cpp)
├── lifecycle_tests (links archive + 1 test .cpp)
├── lifecycle_integration_tests (links archive + 1 test .cpp)
├── rpc_integration_tests (links archive + 1 test .cpp)
└── p2p_sync_integration_tests (links archive + 1 test .cpp)
```

## State Transitions

N/A — build artifacts are stateless; they are produced fresh on each build.

## Validation Rules

- The archive MUST contain exactly the 11 core source files (no more, no less)
- `CliParser.cpp` MUST NOT be in the archive (it depends on Boost.ProgramOptions)
- Every binary that previously compiled `../src/*.cpp` directly MUST instead link `libblockchain_core.a`
- All existing tests MUST pass identically after restructuring
