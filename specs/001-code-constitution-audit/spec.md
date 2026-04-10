# Feature Specification: Code Constitution Audit & Remediation

**Feature Branch**: `001-code-constitution-audit`
**Created**: 2026-04-10
**Status**: Draft
**Input**: User description: "Analyze the code for instances where it strays from the constitution and fix them. Also look for bugs, duplicate code, or inefficiencies."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Enforce C++20 Compilation Standard (Priority: P1)

A developer clones the repository and runs the build. The build system enforces C++20 as the language standard across all targets (main binary and test binary), and `configure` validates that the compiler supports C++20 before proceeding.

**Why this priority**: The constitution designates C++20 as a mandatory language standard (Principle I). Both `src/Makefile.am` and `tests/Makefile.am` currently specify `-std=c++17`, which is a direct constitutional violation. This is the foundation for all other code changes.

**Independent Test**: Build the project from a clean checkout and confirm that both the main binary and the test binary compile under `-std=c++20`. Verify that `configure` fails gracefully on a compiler without C++20 support.

**Acceptance Scenarios**:

1. **Given** a clean checkout, **When** the developer runs `./configure && make`, **Then** both `blockchain` and `blockchain_tests` compile with `-std=c++20` flags.
2. **Given** a compiler that does not support C++20, **When** the developer runs `./configure`, **Then** `configure` exits with a clear error message indicating C++20 is required.

---

### User Story 2 - Harden TLS for All Network Communication (Priority: P1)

A node operator starts the blockchain daemon. All network connections (JSON-RPC and P2P) use properly configured TLS. Certificate file paths are configurable rather than hard-coded to snakeoil defaults. The SSL context enables peer certificate verification.

**Why this priority**: The constitution declares Mandatory TLS as NON-NEGOTIABLE (Principle VI). The current code hard-codes snakeoil certificate paths and performs no peer certificate verification, which weakens TLS and is explicitly forbidden.

**Independent Test**: Start the daemon with configurable certificate paths. Attempt a connection without a valid certificate and confirm it is rejected. Inspect the SSL context to verify peer verification is enabled.

**Acceptance Scenarios**:

1. **Given** valid TLS certificate and key files, **When** the operator sets `BLOCKCHAIN_CERT_FILE` and `BLOCKCHAIN_KEY_FILE` (via environment or `.env` file) and starts the daemon, **Then** both the RPC and P2P servers use the provided certificates.
2. **Given** a peer node connecting to the P2P server without a valid certificate, **When** mutual TLS verification is enforced, **Then** the handshake fails and the connection is refused with a logged error.
3. **Given** an RPC client connecting without a client certificate, **When** server-only TLS is configured, **Then** the handshake succeeds (the server presents its certificate but does not require a client certificate).
4. **Given** the environment variables are not set and no `.env` file exists, **When** the operator starts the daemon, **Then** the daemon exits with a clear error message indicating certificate paths are required.

---

### User Story 3 - Fix Build-Breaking Bugs (Priority: P1)

A developer checks out the repository and runs the test suite. The tests compile and run successfully without filename case-sensitivity failures or incorrect exception handling.

**Why this priority**: The test suite currently has case-sensitivity bugs that prevent compilation on case-sensitive filesystems (Linux), and an incorrect `throw new` pattern in Chunk.cpp that prevents exceptions from being caught properly. These are correctness defects that block all development work.

**Independent Test**: Run `make check` on a case-sensitive filesystem. Verify all tests compile and that exception handlers catch Chunk I/O errors correctly.

**Acceptance Scenarios**:

1. **Given** a case-sensitive filesystem (Linux), **When** the developer runs `make check`, **Then** the test binary compiles without errors.
2. **Given** a Chunk save operation that fails (e.g., read-only directory), **When** an exception is thrown, **Then** the exception is caught by standard `catch(const std::runtime_error&)` handlers.

---

### User Story 4 - Fix Cross-Platform Path Handling (Priority: P2)

A developer builds and runs the blockchain on Windows. All filesystem paths are constructed using portable `std::filesystem::path` operations, not string concatenation with hardcoded `/` separators.

**Why this priority**: The constitution requires cross-platform support for Linux, macOS, and Windows (Principle VII). The current code uses string concatenation with `/` separators in Chunk.cpp and Blockchain.cpp, which fails on Windows.

**Independent Test**: Build on Windows and perform chunk save/load and key save/load operations. Verify paths use the correct platform separator. On any platform, verify that `std::filesystem::path` operators are used instead of string concatenation.

**Acceptance Scenarios**:

1. **Given** any supported platform, **When** a chunk is saved or loaded, **Then** the file path is constructed using `std::filesystem::path` operators (e.g., `/` operator), not string concatenation.
2. **Given** a Windows build, **When** the blockchain directory is `C:\data\chain`, **Then** chunk files are written to `C:\data\chain\chunk_000000.dat` (backslash separator).

---

### User Story 5 - Fix Silent SSL Handshake Failures and Add Timeouts (Priority: P2)

A node operator monitors the blockchain daemon's network connections. When an SSL handshake fails, the error is logged and the connection is cleaned up. When an async read or write operation stalls, the operation times out and the connection is closed.

**Why this priority**: The current code silently drops SSL handshake errors in both RpcServer and PeerServer, leaving connections in an undefined state. No timeout exists on any async operation, creating a denial-of-service vulnerability via slow clients. This violates Principle X (Low-Latency Performance) and weakens the robustness of Principle VI (Mandatory TLS).

**Independent Test**: Simulate an SSL handshake failure and verify the error is logged. Simulate a stalled connection and verify it is closed after a reasonable timeout period.

**Acceptance Scenarios**:

1. **Given** a client that sends an invalid SSL handshake, **When** the handshake fails, **Then** the error is logged to stderr and the connection resources are released.
2. **Given** a client that connects but stops sending data, **When** the timeout period elapses, **Then** the server closes the connection and logs a timeout event.

---

### User Story 6 - Fix Chunk Copy Bug in addBlock (Priority: P2)

A developer adds multiple blocks to the blockchain. Each block is correctly persisted in the underlying chunk data structure, and changes to the working chunk are reflected in the chain vector.

**Why this priority**: `Blockchain::addBlock()` copies the last chunk instead of referencing it (`auto currentChunk = this->chain.back()` instead of `auto& currentChunk`). This means new blocks are added to a temporary copy and silently lost. This is a data-integrity bug.

**Independent Test**: Add multiple blocks to a blockchain instance, then retrieve them by index. Verify all blocks are present and their data is intact.

**Acceptance Scenarios**:

1. **Given** an empty blockchain, **When** 5 blocks are added sequentially, **Then** all 5 blocks are retrievable via `getBlockByIndex()` and their data matches what was inserted.
2. **Given** a blockchain with 99 blocks, **When** the 100th block is added (triggering a new chunk), **Then** the 100th block is in the new chunk and the previous 99 blocks remain accessible.

---

### User Story 7 - Reduce Duplicate Code in Network Layer (Priority: P3)

A developer reads the RpcServer and PeerServer implementations. The SSL handshake and error handling logic is shared through a common base implementation in SessionHandler, reducing code duplication and ensuring consistent behavior.

**Why this priority**: Both RpcServer and PeerServer contain nearly identical SSL async handshake code. Consolidating this into the SessionHandler base class eliminates duplication and ensures that future TLS improvements (e.g., timeouts, logging) are applied consistently.

**Independent Test**: Verify that both RpcServer and PeerServer still pass all existing tests after the handshake logic is extracted to SessionHandler. Verify that a change to the shared handshake logic affects both server types.

**Acceptance Scenarios**:

1. **Given** the refactored codebase, **When** both RpcServer and PeerServer are constructed and started, **Then** they perform SSL handshakes using the shared SessionHandler base logic.
2. **Given** a change to the shared handshake timeout value, **When** both servers are tested, **Then** both reflect the updated timeout.

---

### User Story 8 - Optimize Multi-Key Block Retrieval (Priority: P3)

A user queries the blockchain for blocks matching multiple keys. The system groups block indices by chunk, loads each chunk at most once, and extracts all matching blocks in a single pass per chunk.

**Why this priority**: The current `getBlocksByKeys()` implementation calls `getBlockByIndex()` for each block, potentially loading and unloading the same chunk repeatedly. This violates Principle X (Low-Latency Performance) for queries spanning many blocks across few chunks.

**Independent Test**: Query for blocks across multiple keys that map to the same chunk. Measure that the chunk is loaded only once (or verify via mock that `loadChunk` is called minimally).

**Acceptance Scenarios**:

1. **Given** 50 blocks in chunk 0 indexed by 10 different keys, **When** a query requests all 10 keys, **Then** chunk 0 is loaded at most once during the query.
2. **Given** blocks spread across 3 chunks, **When** a multi-key query is executed, **Then** each chunk is loaded at most once.

### Edge Cases

- What happens when `addBlock` is called concurrently from multiple network handlers? → **In scope** (FR-013: strand serialization prevents concurrent mutation)
- What happens when a chunk file is corrupted or missing during `loadChunk`? → **Deferred**: Existing error handling is retained; graceful recovery is not in scope for this feature.
- What happens when the SSL certificate file is deleted while the server is running? → **Deferred**: Runtime certificate monitoring is not in scope; the server will fail on next handshake attempt with a TLS error.
- What happens when `getBlocksByKeys` is called with an empty key vector? → **Deferred**: Current behavior (return empty result) is acceptable; no change required.
- What happens when a chunk boundary is hit (block 99 → block 100) during concurrent writes? → **In scope** (FR-013: strand serialization prevents concurrent chunk-boundary mutations)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Build system MUST enforce `-std=c++20` in both `src/Makefile.am` and `tests/Makefile.am`.
- **FR-002**: `configure.ac` MUST validate that the compiler supports C++20 before proceeding.
- **FR-003**: TLS certificate and key file paths MUST be provided via environment variables (`BLOCKCHAIN_CERT_FILE`, `BLOCKCHAIN_KEY_FILE`). A helper function MUST be provided to load variables from a `.env` file located in the blockchain directory (the path supplied as the first command-line argument) into the process environment at startup.
- **FR-004a**: The P2P server MUST use mutual TLS — both the server and connecting peer nodes MUST present and verify certificates.
- **FR-004b**: The RPC server MUST use server-only TLS — the server presents its certificate to clients, but client certificates are not required.
- **FR-005**: All `#include` directives and Makefile source references MUST use correct filename casing.
- **FR-006**: Exception throws MUST use value semantics (`throw std::runtime_error(...)`) not heap allocation (`throw new`).
- **FR-007**: All filesystem paths MUST be constructed using `std::filesystem::path` operators, not string concatenation with `/`.
- **FR-008**: SSL handshake failures MUST be logged to stderr using structured output (timestamp + severity level + message) and connection resources MUST be released. No new logging dependency may be added.
- **FR-009**: All async network operations MUST have a configurable timeout defaulting to 30 seconds. Connections that stall beyond the timeout MUST be closed and logged.
- **FR-010**: `Blockchain::addBlock()` MUST use a reference to the chain's backing chunk, not a copy.
- **FR-011**: The SSL async handshake pattern MUST be consolidated into the SessionHandler base class.
- **FR-012**: `getBlocksByKeys()` MUST group block indices by chunk and load each chunk at most once per query.
- **FR-013**: Blockchain and Chunk mutation operations (`addBlock`, `saveChunk`, `loadChunk`, `saveKeys`, `loadKeys`) MUST be protected against concurrent access using mutex or Boost.Asio strand serialization.

### Key Entities

- **Block**: The fundamental data unit; has index, timestamp, data, prevHash, and hash. Must be correctly serializable and hashable.
- **Chunk**: A contiguous group of up to 100 blocks. Manages persistence (save/load) for its block range. Path handling must be cross-platform.
- **Blockchain**: Template class managing the chain of chunks, key-index mapping, and block retrieval. Must maintain data integrity through correct reference semantics.
- **SessionHandler**: Base class for network session management. Should own the shared SSL handshake and timeout logic.
- **RpcServer / PeerServer**: Concrete session handlers for JSON-RPC and P2P protocols respectively. Should delegate common TLS behavior to SessionHandler.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The project compiles cleanly under `-std=c++20` on Linux, macOS, and Windows with zero warnings from `-Wall -Wextra -pedantic`.
- **SC-002**: `make check` passes on all three platforms with all existing and new tests green.
- **SC-003**: No network endpoint accepts a connection without a completed TLS handshake.
- **SC-004**: All SSL handshake failures and timeout events produce a structured log entry (timestamp, severity, message) on stderr visible to the operator.
- **SC-005**: An async operation that stalls for longer than the configured timeout (default 30 seconds) is terminated and logged.
- **SC-006**: Adding 200 blocks sequentially results in all 200 being retrievable by index with correct data.
- **SC-007**: A multi-key query spanning N chunks triggers at most N chunk load operations.
- **SC-008**: All filesystem operations produce correct paths on Windows (backslash separators, drive letter support).
- **SC-009**: Zero constitution violations remain after remediation (verified by reviewing all 11 principles against the updated code).
- **SC-010**: Concurrent `addBlock` calls from multiple network handlers do not corrupt the chain or cause undefined behavior.

## Clarifications

### Session 2026-04-10

- Q: What TLS verification model should the server endpoints use? → A: P2P uses mutual TLS (both sides present certificates); RPC uses server-only TLS (server presents certificate, no client certificate required).
- Q: What should the default async operation timeout be? → A: 30 seconds.
- Q: What logging approach should the project use for errors and diagnostics? → A: Structured logging (timestamp + severity + message) to stderr, no new dependency.
- Q: Is thread safety for Blockchain data structures in scope? → A: Yes — add mutex/strand protection to Blockchain and Chunk mutation operations now.
- Q: How should TLS certificate paths be provided to the daemon? → A: Environment variables (`BLOCKCHAIN_CERT_FILE`, `BLOCKCHAIN_KEY_FILE`) with a helper function to load from a `.env` file in the blockchain directory (the command-line argument).

## Assumptions

- The existing test suite (Catch2) is functional and `make check` is the standard way to run tests.
- Snakeoil (self-signed) certificates are acceptable for local development and testing, but developers must explicitly configure certificate paths (even for self-signed certs) via environment variables or `.env` file. The code must not hard-code any certificate paths.
- The `MockChunk`, `MockSessionHandler`, and `MockAcceptor` test doubles are correct and can be extended for new test scenarios.
- The P2P binary protocol's `PacketHeader` structure is stable enough for integration testing even though the protocol may evolve (Principle IX).
- Cross-platform CI is not yet set up; Windows compatibility will be validated by code-level review of path handling and build definitions until CI is available.
- The `isBlockPresent` logic in IChunk is acceptable given the current sequential-index invariant, but should be documented as depending on that invariant.
