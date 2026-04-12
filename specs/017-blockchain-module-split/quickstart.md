# Quickstart: Blockchain Module Split

**Feature**: 017-blockchain-module-split

## What Changed

The monolithic `Blockchain.cpp` (1,019 lines) is split into four focused modules:

| Module | File | Lines | Responsibility |
|--------|------|-------|---------------|
| **ChainPersistence** | `ChainPersistence.hpp/cpp` | ~340 | Chunk I/O, index files, recovery, archiving |
| **DifficultyEngine** | `DifficultyEngine.hpp/cpp` | ~100 | Difficulty calculation and caching |
| **MerkleProofService** | `MerkleProofService.hpp/cpp` | ~50 | Inclusion proof generation and verification |
| **Blockchain (core)** | `Blockchain.hpp/cpp` | ~435 | Chain ops, stream ops, coordination |

## Architecture

```
Blockchain<ChunkHandler>  (owns all state, implements IBlockchain)
├── persistence_     : ChainPersistence<ChunkHandler>
├── difficultyEngine_ : DifficultyEngine
└── proofService_     : MerkleProofService
```

- Core owns all shared state (chain, indexes, caches, flags)
- Modules receive references to state they need per method call
- Only ChainPersistence is templated on `ChunkHandler`
- IBlockchain interface is unchanged

## Building

No new build steps. Existing workflow:

```bash
make -j8
```

New `.cpp` files are added to `libblockchain_core_a_SOURCES` in `src/Makefile.am`.

## Testing

All existing tests continue to pass. New focused test binaries:

```bash
./tests/difficulty_engine_tests
./tests/chain_persistence_tests
./tests/merkle_proof_tests
```

## For Developers

- **Modifying persistence logic?** → Edit `ChainPersistence.cpp` only
- **Changing difficulty algorithm?** → Edit `DifficultyEngine.cpp` only
- **Updating Merkle proofs?** → Edit `MerkleProofService.cpp` only
- **Adding chain operations?** → Edit `Blockchain.cpp` (core)
