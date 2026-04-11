# Implementation Plan: Merkle Tree & Block Header Optimization

**Branch**: `008-merkle-block-headers` | **Date**: 2026-04-11 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/008-merkle-block-headers/spec.md`

## Summary

Add per-block Merkle trees over stream entries using RFC 6962 domain-separated SHA-256 hashing. Each block gains a `merkleRoot` field that replaces raw serialized entry data in the block hash computation. New `MerkleTree` module provides tree construction, proof generation, and proof verification. Three new JSON-RPC endpoints (`getInclusionProof`, `verifyInclusionProof`, `getBlockHeader`) expose Merkle proofs and lightweight headers to clients.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`)
**Storage**: Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`)
**Testing**: Catch2, `make check`
**Target Platform**: Linux, macOS, Windows
**Project Type**: Library/daemon
**Performance Goals**: O(log n) Merkle proof generation and verification; ≤7 hashes deep for typical 1–100 entry blocks
**Constraints**: SHA-256 only (OpenSSL EVP), no new dependencies, all network traffic over TLS
**Scale/Scope**: 1–100 entries per block, 100 blocks per chunk

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. C++20 | PASS | All new code uses `-std=c++20` |
| II. GNU Autotools | PASS | New files added to existing `Makefile.am` |
| III. Full Test Coverage | PASS | Merkle unit tests + updated block tests planned |
| IV. Code Style | PASS | Follows existing conventions |
| V. Minimal Dependencies | PASS | No new dependencies; uses existing OpenSSL SHA-256 and Boost.Serialization |
| VI. Mandatory TLS | PASS | No network changes; new RPC endpoints served over existing TLS |
| VII. Cross-Platform | PASS | Pure C++20 + Boost + OpenSSL; no platform-specific code |
| VIII. Feature Branches | PASS | On branch `008-merkle-block-headers` |
| IX. Pre-1.0 API | PASS | New RPC methods added freely; JSON response extension is non-breaking |
| X. Low-Latency | PASS | Merkle root computation O(n), proof O(log n); replaces O(n) entry serialization in hash |
| XI. MIT License | PASS | No new third-party code |

**Post-Phase 1 re-check**: All gates still PASS. No new dependencies, no platform-specific code, no TLS changes.

## Project Structure

### Documentation (this feature)

```text
specs/008-merkle-block-headers/
├── plan.md              # This file
├── research.md          # Phase 0: research decisions
├── data-model.md        # Phase 1: entity definitions
├── quickstart.md        # Phase 1: validation guide
├── contracts/
│   └── json-rpc.md      # Phase 1: RPC endpoint contracts
└── tasks.md             # Phase 2 output (created by /speckit.tasks)
```

### Source Code (repository root)

```text
src/
├── Block.hpp            # Modified: add merkleRoot field, update calculateHash(), toJson(), toHeaderJson(), serialize()
├── Block.cpp            # Modified: update calculateHash() to use merkleRoot, add toHeaderJson()
├── MerkleTree.hpp       # NEW: Merkle tree construction, proof generation, proof verification
├── MerkleTree.cpp       # NEW: implementation
├── Blockchain.hpp       # Modified: add getInclusionProof(), verifyInclusionProof() methods
├── Blockchain.cpp       # Modified: compute merkleRoot during block creation
├── IBlockchain.hpp      # Modified: add proof/header methods to interface
├── network/
│   └── RpcServer.cpp    # Modified: add getInclusionProof, verifyInclusionProof, getBlockHeader handlers
├── Makefile.am          # Modified: add MerkleTree.cpp to sources

tests/
├── merkle_tests.cpp     # NEW: Merkle tree unit tests (construction, proofs, edge cases)
├── block_tests.cpp      # Modified: update for merkleRoot in block hash and serialization
├── Makefile.am          # Modified: add merkle_tests.cpp to sources
```

**Structure Decision**: Single project layout matching existing `src/` and `tests/` convention. New `MerkleTree` module added as a standalone compilation unit alongside existing source files.
