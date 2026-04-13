# Research: Blockchain Module Split

**Date**: 2026-04-12  
**Feature**: 017-blockchain-module-split

## Function-to-Module Mapping

Analysis of all 28 functions in Blockchain.cpp (1,019 lines), mapped to target modules:

### ChainPersistence (template\<ChunkHandler\>) — ~340 lines estimated

| Function | Lines | Key State Access |
|----------|-------|-----------------|
| `saveChunk()` | 282–285 | chain[i] → disk |
| `loadChunk()` | 291–293 | disk → chain[i] |
| `freeChunk()` | 287–289 | chain[i], retainedChunks_ |
| `saveKeys()` | 295–300 | keyIndexMap → keys.dat |
| `loadKeys()` | 302–309 | keys.dat → keyIndexMap |
| `saveStreams()` | 311–316 | streamRegistry → streams.dat |
| `loadStreams()` | 318–325 | streams.dat → streamRegistry |
| `saveStreamIndex()` | 327–332 | streamKeyIndex → stream_index.dat |
| `loadStreamIndex()` | 334–341 | stream_index.dat → streamKeyIndex |
| `saveAllChunks()` | 378–410 | chain, indexes, dirty_ |
| `discoverChunks()` | 412–420 | blockchainPath |
| `validateChunk()` | 422–468 | blockchainPath, config |
| `recoverChain()` | 470–578 | All state (recovery is full-state rebuild) |
| `archiveChainFiles()` | 580–621 | blockchainPath, chunkCount_ |

Total: 14 functions, ~340 raw lines

### DifficultyEngine (non-template) — ~100 lines estimated

| Function | Lines | Key State Access |
|----------|-------|-----------------|
| `calculateNewDifficulty()` | 723–761 | config, totalBlockCount_, currentDifficulty |
| `getDifficultyForHeight()` | 763–816 | config, difficultyCache_, block data (via callback) |

Total: 2 functions, ~94 raw lines

### MerkleProofService (non-template) — ~50 lines estimated

| Function | Lines | Key State Access |
|----------|-------|-----------------|
| `getInclusionProof()` | 818–843 | block data (entries), config |
| `verifyInclusionProof()` | 845–862 | block data (merkleRoot) |

Total: 2 functions, ~45 raw lines

### Blockchain Core (template\<ChunkHandler\>) — ~435 lines estimated

| Function | Lines | Key State Access |
|----------|-------|-----------------|
| `generateGenesisBlock()` | 22–26 | chain, totalBlockCount_, chunkCount_ |
| `publish()` | 28–113 | All core state + delegates save/difficulty |
| `createStream()` | 115–120 | streamRegistry |
| `listStreams()` | 122–125 | streamRegistry |
| `getStreamEntries()` | 127–161 | streamKeyIndex, chain |
| `getStreamEntry()` | 163–183 | streamKeyIndex, chain |
| `appendBlock()` | 185–221 | chain, indexes, delegates save/difficulty |
| `getBlockByIndex()` | 223–244 | chain (load/free) |
| `getBlocksByKeys()` | 246–280 | chain, keyIndexMap |
| `getChainBlockCount()` | 363–366 | totalBlockCount_ |
| `getChainLength()` | 368–371 | totalBlockCount_ |
| `getChunkCount()` | 373–376 | chunkCount_ |
| `isValidChain()` | 658–671 | config |
| `replaceChain()` | 673–721 | All state + delegates archive/difficulty |
| `dumpBlocks()` | 343–351 | chain |
| `dumpKeys()` | 353–361 | keyIndexMap |
| `startPeriodicSave()` | 623–650 | io_context_, save_timer_, dirty_ |
| `stopPeriodicSave()` | 652–656 | save_timer_ |

Total: 18 functions, ~435 raw lines

## Composition Pattern — Decision

**Decision**: Composition with reference passing  
**Rationale**: Blockchain core owns all shared state and module instances. Modules receive references to the state they need via constructor or method parameters.  
**Alternatives considered**:
- Inheritance (rejected: no IS-A relationship; would produce a complex diamond hierarchy)
- Free functions (rejected: stateful modules like DifficultyEngine need to hold cache state)
- Dependency injection via interfaces (rejected: over-engineering for 3 internal modules; adds abstraction without value since modules are not swapped at runtime)

## Template Propagation — Decision

**Decision**: Selective templating — only ChainPersistence is templated on `ChunkHandler`  
**Rationale**: ChainPersistence directly manipulates `std::vector<ChunkHandler>` for save/load/free/recover. DifficultyEngine and MerkleProofService operate on `Block` data passed by value or reference, with no need to know the chunk storage type.  
**Alternatives considered**:
- All templated (rejected: DifficultyEngine and MerkleProofService don't touch chunks; unnecessary template bloat)
- None templated with abstract interface (rejected: would require a new IChunkStore interface just to hide the template, adding complexity)

## State Ownership — Decision

**Decision**: Blockchain core retains all state; modules receive references  
**Rationale**: The monolith's state is highly interconnected — `recoverChain()` touches every field, `publish()` and `appendBlock()` update multiple concerns. Splitting state ownership would require complex synchronization or notification patterns between modules. Reference passing keeps a single source of truth.  
**Alternatives considered**:
- State migration to modules (rejected: would require bidirectional references and complex ownership semantics)
- Shared context struct (rejected: creates a public god-object; worse than current private fields)

## Module Interface Design — Decision

**Decision**: Modules receive what they need as method parameters, not via constructors storing broad references  
**Rationale**: Narrower interfaces per method call make dependencies explicit. For example, `DifficultyEngine::calculateNewDifficulty()` takes config, blockCount, currentDifficulty — callers can see exactly what it needs. This also makes unit testing trivial (pass values, assert result).  
**Exception**: ChainPersistence constructor takes `blockchainPath` and `chunkSize` since these are invariant configuration used by every persistence method.

## `recoverChain()` — Cross-Cutting Concern

`recoverChain()` (109 lines) touches all state: chunks, indexes, difficulty cache, counters. It must remain aware of all modules.

**Decision**: `recoverChain()` stays in ChainPersistence but receives references to all state it needs to rebuild. The core Blockchain calls `persistence_.recoverChain(chain, ...)` passing all mutable state. This keeps recovery logic centralized in the persistence module (where it belongs — it's disk recovery) while the core orchestrates what state to pass.

## Build Integration — Research

**Current**: `src/Makefile.am` defines `libblockchain_core.a` with 10 source files.  
**Change**: Add 3 new source files to `libblockchain_core_a_SOURCES`:
- `ChainPersistence.cpp`
- `DifficultyEngine.cpp`
- `MerkleProofService.cpp`

Tests: Add 3 new test binaries in `tests/Makefile.am` following the existing pattern.

No Autotools regeneration needed for adding `.cpp` files to existing targets — `make -j8` after `Makefile.am` edit is sufficient.

## Risk Assessment

| Risk | Mitigation |
|------|-----------|
| Template instantiation errors after split | Explicit template instantiation in `.cpp` files (existing pattern with `Chunk` and `MockChunk`) |
| Circular includes between modules | Modules depend on Block.hpp, StreamEntry.hpp, ConsensusConfig.hpp — not on each other |
| Performance regression from indirection | Function call overhead is negligible; all calls are inline-eligible |
| Test breakage from header changes | Blockchain.hpp includes new module headers; test files include Blockchain.hpp unchanged |
