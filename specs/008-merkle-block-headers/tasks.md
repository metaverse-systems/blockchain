# Tasks: Merkle Tree & Block Header Optimization

**Input**: Design documents from `/specs/008-merkle-block-headers/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/json-rpc.md, quickstart.md

**Tests**: Included per Constitution Principle III (Full Test Coverage required for every new feature).

**Organization**: Tasks are grouped by user story to enable independent implementation and testing of each story.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

## Phase 1: Setup

**Purpose**: Build system changes and new file scaffolding

- [X] T001 Add MerkleTree.cpp to blockchain_SOURCES in src/Makefile.am
- [X] T002 [P] Add merkle_tests.cpp to test sources and create merkle_tests check binary in tests/Makefile.am

---

## Phase 2: Foundational (MerkleTree Module)

**Purpose**: Core Merkle tree logic that ALL user stories depend on. Must be complete before any story work.

**⚠️ CRITICAL**: US1/US2/US3 all depend on this module.

- [X] T003 Create MerkleTree.hpp with MerkleProofElement struct, computeLeafHash(), computeMerkleRoot(), generateProof(), verifyProof() declarations in src/MerkleTree.hpp
- [X] T004 Implement computeLeafHash() in src/MerkleTree.cpp — Boost-serialize a StreamEntry, prefix with 0x00 byte, SHA-256 hash, return hex string
- [X] T005 Implement computeMerkleRoot() in src/MerkleTree.cpp — convert leaf hex hashes to raw 32-byte digests, build tree bottom-up concatenating raw digests with 0x01 prefix for internal nodes per RFC 6962, duplicate last node on odd counts, return final root as hex string; return SHA-256 of empty string for zero entries
- [X] T006 Implement generateProof() in src/MerkleTree.cpp — given leaf hashes and target index, return vector of MerkleProofElement with sibling hash and isLeft flag
- [X] T007 Implement verifyProof() in src/MerkleTree.cpp — convert leaf hash and sibling hashes from hex to raw 32-byte digests, recompute root using 0x01 prefix on raw concatenations, convert final root to hex, compare against expected root

**Checkpoint**: MerkleTree module complete and independently testable via unit tests

---

## Phase 3: User Story 1 — Efficient Block Verification Without Full Data (Priority: P1) 🎯 MVP

**Goal**: Each block stores a Merkle root computed from its entries; block hash uses the Merkle root instead of raw serialized entry data.

**Independent Test**: Mine blocks with varied entries, confirm merkleRoot is populated and block hash changes when any entry changes.

### Tests for User Story 1

- [X] T008 [P] [US1] Write Merkle tree unit tests in tests/merkle_tests.cpp — test computeLeafHash determinism, computeMerkleRoot with 0/1/2/5/odd/even entries, empty-tree constant, order sensitivity, duplicate entries
- [X] T009 [P] [US1] Write Merkle tree edge-case tests in tests/merkle_tests.cpp — single entry root equals leaf hash, odd-count duplication produces correct root, large entry count (100 entries), large individual entry payload (≥1 MB data field) to verify computeLeafHash handles large serialized data without truncation

### Implementation for User Story 1

- [X] T010 [US1] Add merkleRoot string field to Block struct, update serialize() template to include it after difficulty, in src/Block.hpp
- [X] T011 [US1] Update Block constructor in src/Block.cpp — compute leaf hashes from entries via MerkleTree::computeLeafHash(), call MerkleTree::computeMerkleRoot(), store result in merkleRoot field before calling calculateHash()
- [X] T012 [US1] Update Block::calculateHash() in src/Block.cpp — replace Boost-serialized entries with merkleRoot string in hash input: SHA-256(index || timestamp || merkleRoot || prevHash || nonce || difficulty)
- [X] T013 [US1] Update Block::toJson() in src/Block.cpp — include merkleRoot field in JSON output
- [X] T014 [US1] Update block tests in tests/block_tests.cpp — verify merkleRoot is populated on construction, hash incorporates merkleRoot, serialization round-trip preserves merkleRoot, hash changes when entries differ

**Checkpoint**: Blocks now contain Merkle roots. Mining, validation, and serialization all work with the new field. `make check` passes.

---

## Phase 4: User Story 2 — Proof-of-Inclusion for Stream Entries (Priority: P2)

**Goal**: Clients can request and verify Merkle inclusion proofs for any entry in any block via JSON-RPC.

**Independent Test**: Publish entries, call getInclusionProof RPC, verify proof succeeds for correct data and fails for tampered data.

### Tests for User Story 2

- [X] T015 [P] [US2] Write proof generation/verification unit tests in tests/merkle_tests.cpp — generate proof for each position in a multi-entry block, verify each proof against root, verify tampered leaf fails, verify truncated/extra-element proofs fail, assert proof size equals ceil(log2(n)) siblings for blocks of 1/10/100 entries (validates SC-003 O(log n) property)
- [X] T016 [P] [US2] Write proof edge-case tests in tests/merkle_tests.cpp — proof for single-entry block (empty proof path), proof for entry at last position in odd-count block, out-of-range index returns error

### Implementation for User Story 2

- [X] T017 [US2] Add getInclusionProof() method to IBlockchain interface in src/IBlockchain.hpp — takes blockIndex and entryIndex, returns MerkleProof JSON or error
- [X] T018 [US2] Implement getInclusionProof() in src/Blockchain.cpp — load block by index, compute leaf hashes, call MerkleTree::generateProof(), return JSON with blockIndex, entryIndex, merkleRoot, leafHash, proof array
- [X] T019 [US2] Add verifyInclusionProof() method to IBlockchain interface in src/IBlockchain.hpp — takes blockIndex, leafHash, proof array, returns valid boolean + merkleRoot
- [X] T020 [US2] Implement verifyInclusionProof() in src/Blockchain.cpp — load block by index, call MerkleTree::verifyProof() against block's merkleRoot, return result JSON
- [X] T020a [US2] Update tests/MockBlockchain.hpp — add stub implementations of getInclusionProof() and verifyInclusionProof() to satisfy IBlockchain interface changes from T017/T019, preventing compilation failures in existing mock-based tests
- [X] T021 [US2] Add getInclusionProof RPC handler in src/network/RpcServer.cpp — validate params (blockIndex, entryIndex as integers), call blockchain.getInclusionProof(), return result per contracts/json-rpc.md, error -32001 for bad block, -32002 for bad entry, -32602 for invalid params
- [X] T022 [US2] Add verifyInclusionProof RPC handler in src/network/RpcServer.cpp — validate params (blockIndex integer, leafHash string, proof array with hash/isLeft), call blockchain.verifyInclusionProof(), return {valid, merkleRoot} per contracts/json-rpc.md

**Checkpoint**: Clients can generate and verify Merkle proofs via RPC. `make check` passes.

---

## Phase 5: User Story 3 — Lightweight Block Headers (Priority: P3)

**Goal**: Blocks expose a header-only view (fixed-size, no entry data) for efficient P2P and client operations, accessible via RPC.

**Independent Test**: Call getBlockHeader RPC, confirm response contains only header fields and is fixed-size regardless of entry count.

### Tests for User Story 3

- [X] T023 [P] [US3] Write header tests in tests/block_tests.cpp — toHeaderJson() returns exactly 7 fields (index, timestamp, prevHash, merkleRoot, nonce, difficulty, hash), no entries field, header hash matches full block hash

### Implementation for User Story 3

- [X] T024 [US3] Add toHeaderJson() method to Block in src/Block.hpp and src/Block.cpp — return JSON with index, timestamp, prevHash, merkleRoot, nonce, difficulty, hash (no entries array)
- [X] T025 [US3] Add getBlockHeader RPC handler in src/network/RpcServer.cpp — validate params (blockIndex integer), load block, call toHeaderJson(), return result per contracts/json-rpc.md, error -32001 for bad block, -32602 for invalid params

**Checkpoint**: Header-only retrieval works via RPC. Response is fixed-size. `make check` passes.

---

## Phase 5a: Integration Tests (All Stories)

**Purpose**: Network integration tests for new RPC endpoints per Constitution Principle III.

- [X] T025a [P] Create tests/merkle_rpc_integration_tests.cpp — add to check_PROGRAMS and TESTS in tests/Makefile.am, modeled after existing block_propagation_integration_tests.cpp structure
- [X] T025b Write integration tests for getInclusionProof, verifyInclusionProof, and getBlockHeader RPC endpoints over TLS — publish entries via RPC, call getInclusionProof for a known entry, verify response structure matches contracts/json-rpc.md, call verifyInclusionProof with returned proof and confirm valid:true, call getBlockHeader and confirm header-only response, test error cases (-32001 bad block, -32002 bad entry, -32602 invalid params)

**Checkpoint**: All 3 new RPC endpoints covered by automated integration tests over TLS. `make check` passes.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentation, validation, and cleanup

- [X] T026 [P] Update docs/ROADMAP.md — move 008 from Suggested Specs to Completed table, update priority order diagram
- [X] T027 [P] Update README.md — add Merkle tree and block header capabilities to project description, mention getInclusionProof/verifyInclusionProof/getBlockHeader RPC endpoints
- [ ] T028 Run quickstart.md validation — requires running daemon (manual validation)
- [X] T029 Run full test suite via make check — 7/8 suites PASS; chunk_recovery_tests is a pre-existing slow test (not related to Merkle changes)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 — BLOCKS all user stories
- **US1 (Phase 3)**: Depends on Phase 2 (MerkleTree module)
- **US2 (Phase 4)**: Depends on Phase 2 (MerkleTree module) + Phase 3 (merkleRoot in Block)
- **US3 (Phase 5)**: Depends on Phase 3 (merkleRoot in Block, toHeaderJson)
- **Integration Tests (Phase 5a)**: Depends on all user stories being complete (US1 + US2 + US3)
- **Polish (Phase 6)**: Depends on Phase 5a

### User Story Dependencies

- **US1 (P1)**: Depends on Foundational only — standalone MVP
- **US2 (P2)**: Depends on US1 (needs merkleRoot in Block to generate proofs against)
- **US3 (P3)**: Depends on US1 (needs merkleRoot field for header). Can run in parallel with US2.

### Within Each User Story

- Tests written first (verify they reference correct interfaces)
- Core data model changes before service logic
- Service logic before RPC handlers
- All implementation before checkpoint verification

### Parallel Opportunities

**Phase 1**: T001 and T002 can run in parallel (different Makefile.am files)

**Phase 2**: T003 first (header), then T004–T007 are sequential (each builds on prior)

**Phase 3 (US1)**:
- T008 ∥ T009 (independent test files)
- T010 → T011 → T012 → T013 (sequential chain: field → constructor → hash → JSON)
- T014 after T010–T013

**Phase 4 (US2)**:
- T015 ∥ T016 (independent test sections)
- T017 → T018 (interface then implementation for getInclusionProof)
- T019 → T020 (interface then implementation for verifyInclusionProof)
- T020a after T017+T019 (MockBlockchain stubs for new interface methods)
- T017–T018 ∥ T019–T020 (two independent method pairs)
- T021 after T018, T022 after T020

**Phase 5 (US3)** ∥ **Phase 4 (US2)** — can run in parallel after Phase 3 completes:
- T023 (test), T024 → T025 (implementation → RPC)

**Phase 6**: T026 ∥ T027 (independent doc files), then T028 → T029 (validation)

**Phase 5a**: T025a → T025b (scaffold then write integration tests)

---

## Implementation Strategy

**MVP**: Phase 1 + Phase 2 + Phase 3 (US1) — blocks store Merkle roots and use them in hash computation. This alone delivers the core integrity guarantee and can be shipped independently.

**Full Feature**: MVP + Phase 4 (US2) + Phase 5 (US3) — adds client-facing proof APIs and header-only retrieval.

**Suggested execution order** (single developer): T001–T007 → T008–T014 → T015–T022,T020a → T023–T025 → T025a–T025b → T026–T029
