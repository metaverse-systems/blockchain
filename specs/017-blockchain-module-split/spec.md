# Feature Specification: Blockchain Module Split

**Feature Branch**: `017-blockchain-module-split`  
**Created**: 2026-04-12  
**Status**: Draft  
**Input**: User description: "Implement 6.2 — Split Blockchain.cpp into focused modules (per code audit §6.2)"

## Clarifications

### Session 2026-04-12

- Q: How do extracted modules relate structurally to the core Blockchain class? → A: Composition — Blockchain owns module instances as members and delegates to them.
- Q: Are extracted modules templated on ChunkHandler like Blockchain? → A: Selective templating — only ChainPersistence is templated on ChunkHandler; DifficultyEngine and MerkleProofService are non-template classes.
- Q: Which component owns shared state (chain, dirty flag, caches, etc.)? → A: Blockchain core retains ownership of all shared state; modules receive references to operate on it.
- Q: Where does stream query logic (getStreamEntries, createStream, etc.) live after the split? → A: Stream queries remain in the core Blockchain module alongside publish/append/replace.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Independent Module Maintenance (Priority: P1)

As a developer working on block persistence logic, I can modify persistence-related code without navigating or risking changes to unrelated consensus, mining, or proof-generation code. Each module has clear boundaries so that a change to how chunks are saved does not require understanding difficulty adjustment or Merkle proof generation.

**Why this priority**: The primary pain point identified in the audit is that a single 1,024-line file mixes persistence, consensus, difficulty, mining, proofs, and queries. Reducing cognitive load and merge-conflict risk for the most common maintenance tasks delivers the highest value.

**Independent Test**: Can be verified by confirming that persistence-related changes (save, load, recover, archive) compile and pass tests without touching difficulty or proof modules.

**Acceptance Scenarios**:

1. **Given** the blockchain codebase, **When** a developer modifies persistence logic, **Then** only the persistence module and its tests need to be rebuilt and reviewed.
2. **Given** the blockchain codebase, **When** two developers work on persistence and difficulty simultaneously, **Then** their changes do not produce merge conflicts in the same file.

---

### User Story 2 - Isolated Difficulty Adjustment Changes (Priority: P2)

As a developer tuning the difficulty adjustment algorithm, I can locate, understand, and modify all difficulty-related logic in a single, focused module without sifting through persistence, mining, or query code.

**Why this priority**: Difficulty adjustment is a critical consensus parameter. Isolating it reduces the risk of accidental side effects when consensus rules evolve.

**Independent Test**: Can be verified by confirming that difficulty-related logic lives in a single module that can be tested in isolation with mock chain data.

**Acceptance Scenarios**:

1. **Given** the difficulty module, **When** the adjustment algorithm is changed, **Then** only difficulty-related tests need updating.
2. **Given** a test harness with mock block data, **When** difficulty calculations are exercised, **Then** no persistence or network code is invoked.

---

### User Story 3 - Standalone Merkle Proof Testing (Priority: P3)

As a developer or auditor, I can verify Merkle proof generation and verification logic independently of block storage or chain operations, enabling targeted security review and testing.

**Why this priority**: Merkle proofs are a cryptographic integrity mechanism. Isolating them allows focused security audits and test coverage without exercising the full chain stack.

**Independent Test**: Can be verified by running proof generation and verification tests that operate on in-memory block data without touching disk persistence.

**Acceptance Scenarios**:

1. **Given** a set of block entries, **When** a Merkle proof is generated and verified, **Then** the operation completes without loading or saving any chunks.
2. **Given** the proof module, **When** a security reviewer audits proof logic, **Then** the relevant code is contained in a single focused module.

---

### User Story 4 - Reduced Build Times for Focused Changes (Priority: P3)

As a developer making a small change to one concern (e.g., persistence), I experience faster incremental builds because only the affected module is recompiled rather than the entire monolithic file.

**Why this priority**: Build speed directly affects developer productivity, especially during iterative development and debugging cycles.

**Independent Test**: Can be verified by timing incremental builds after a single-module change versus the current monolithic rebuild.

**Acceptance Scenarios**:

1. **Given** a change to only the persistence module, **When** an incremental build runs, **Then** unrelated modules (difficulty, proofs, core chain) are not recompiled.

---

### Edge Cases

- What happens when a module needs data owned by another module (e.g., difficulty adjustment needs block timestamps from persistence)?
- How does the system behave when persistence operations fail partway through and other modules query stale cached state?
- What happens when chain replacement triggers updates across all modules simultaneously?

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST separate block persistence operations (save, load, recover, archive, free chunks) into a dedicated module distinct from core chain logic.
- **FR-002**: The system MUST separate difficulty calculation and adjustment logic into a dedicated module that can operate on block data without direct persistence coupling.
- **FR-003**: The system MUST separate Merkle proof generation and verification into a dedicated module independent of chain state management.
- **FR-004**: The core blockchain module MUST retain responsibility for chain operations (publish, append, replace), stream operations (create, list, query), and coordinate with the other modules.
- **FR-005**: All existing public interfaces and behaviors MUST remain unchanged after the split — no regressions in functionality.
- **FR-006**: All existing tests MUST continue to pass without modification to test logic (test file organization may change to match new module boundaries).
- **FR-007**: Each module MUST have well-defined boundaries so that changes to one module's internals do not require changes to another module's internals. Modules do not own shared state; the core Blockchain class retains all state ownership and passes references to modules as needed.
- **FR-008**: The system MUST maintain the current single-threaded execution guarantee across all modules.

### Key Entities

- **ChainPersistence**: Responsible for chunk save/load/recover/archive operations, chunk filename management, and stream/key index persistence. Owned as a member of the core Blockchain class. Templated on `ChunkHandler` to access chunk data directly.
- **DifficultyEngine**: Responsible for difficulty calculation, adjustment window logic, and difficulty caching. Owned as a member of the core Blockchain class. Non-template class; receives block data via function parameters.
- **MerkleProofService**: Responsible for Merkle proof generation and verification from block entries. Owned as a member of the core Blockchain class. Non-template class; receives block data via function parameters.
- **Blockchain (core)**: Responsible for chain operations — publish, append, replace, block/stream queries. Owns instances of ChainPersistence, DifficultyEngine, and MerkleProofService as member objects and delegates to them via composition.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: No single source module exceeds 400 lines of code (down from 1,024 lines).
- **SC-002**: All existing tests pass with zero failures after the module split.
- **SC-003**: A change touching only persistence logic requires rebuilding at most one module plus the core coordinator, not the entire chain codebase.
- **SC-004**: Each extracted module can be tested with mock dependencies, enabling at least 3 new focused test cases per module.
- **SC-005**: Developer onboarding time for understanding a single concern (e.g., "how does difficulty adjustment work?") is reduced by having all relevant logic in one file under 400 lines.

## Assumptions

- The existing `IBlockchain` interface remains unchanged in this feature; narrowing it into reader/writer interfaces (audit §6.4) is deferred to a future feature.
- The existing `Blockchain` template parameterization on `ChunkHandler` is preserved; module extraction does not alter the template design.
- The RPC dispatch refactoring (audit §4.6/§6.1) is out of scope for this feature.
- Modules communicate via direct function calls within the same process; no inter-process or network communication is introduced.
- The single-threaded `io_context` execution model documented in audit §5 is maintained and not affected by the module split.
