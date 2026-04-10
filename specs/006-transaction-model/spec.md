# Feature Specification: Transaction Model

**Feature Branch**: `006-transaction-model`  
**Created**: 2026-04-10  
**Status**: Draft  
**Input**: User description: "Implement 006 — Transaction Model: Define a structured transaction schema so blocks carry verifiable, structured payloads instead of opaque data strings. Use a key/value store model with MultiChain-style streams. Clients submit JSON data with a user-provided key to a named stream."

## Clarifications

### Session 2026-04-10

- Q: What transaction structure should the chain use (sender/recipient transfer model vs key/value store)? → A: Key/value store with MultiChain-style streams. No sender/recipient. Clients send JSON data with a user-provided key to a named stream. No digital signatures in this feature.
- Q: How should streams be created, and should there be access control? → A: Both explicit and implicit creation (publishing to a non-existent stream auto-creates it, but streams can also be pre-created). Per-node permissions control which streams a node allows publishing to.
- Q: When multiple entries share the same stream + key, what should queries return? → A: Two query modes — one returns all entries in chain order (full history), another returns only the latest entry (last-write-wins). Both are available.
- Q: What is the maximum entry size, and should binary data be supported? → A: 128 MB maximum per entry. Binary data is supported via client-side base64 encoding. The node treats all data as opaque strings — no JSON validation or format indicator needed.
- Q: Should streams carry metadata when created? → A: Bare name only — no metadata at creation time. Metadata can be added later via a reserved key if needed.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Publish Data to a Stream (Priority: P1)

As a blockchain client, I want to publish data to a named stream with a user-provided key so that the blockchain acts as a structured, queryable key/value store rather than storing opaque strings.

**Why this priority**: This is the fundamental capability — without the ability to publish structured data to streams, no other stream-related features can function. It transforms the chain from storing arbitrary strings to a structured data store.

**Independent Test**: Can be tested by publishing an entry to a stream via the RPC endpoint and verifying the resulting block contains the correctly serialized stream entry.

**Acceptance Scenarios**:

1. **Given** a client with a valid stream name, key, and data, **When** the client submits a publish request via the `addBlock` RPC, **Then** a new block is created containing the stream entry and appended to the chain.
2. **Given** a client submits a publish request with an empty data field, **When** the node receives the request, **Then** the entry is accepted (empty data is valid).
3. **Given** a client submits a publish request with an empty stream name or key, **When** the node validates the request, **Then** the node rejects it with an error indicating the required field is missing.

---

### User Story 2 - Query Data by Stream and Key (Priority: P2)

As a blockchain client, I want to query entries by stream name, key, or both so that I can retrieve specific data from the chain without scanning all blocks.

**Why this priority**: A key/value store is only useful if data can be efficiently retrieved. Query capability makes the stream model practical for applications.

**Independent Test**: Can be tested by publishing entries to different streams with known keys, then querying by stream, key, or both and verifying correct results.

**Acceptance Scenarios**:

1. **Given** entries exist in stream "assets" with keys "item-1" and "item-2", **When** a client queries for stream "assets" key "item-1" in history mode, **Then** all matching entries are returned in chain order (oldest to newest) with full data.
2. **Given** entries exist in stream "assets" with keys "item-1" and "item-2", **When** a client queries for stream "assets" key "item-1" in latest mode, **Then** only the most recent entry is returned.
3. **Given** entries exist across multiple streams, **When** a client queries for all entries in stream "assets", **Then** all entries in that stream are returned.
4. **Given** multiple entries exist with the same key in the same stream, **When** a client queries in history mode, **Then** entries are returned in chain order. **When** querying in latest mode, **Then** only the last-written entry is returned.

---

### User Story 3 - Validate Stream Entries in Received Blocks (Priority: P2)

As a node operator, I want my node to validate stream entries in blocks received via P2P so that only well-formed entries with valid stream names and keys are accepted.

**Why this priority**: Without validation of received blocks, malformed or corrupted stream entries could pollute the chain.

**Independent Test**: Can be tested by constructing a block with a malformed stream entry and sending it via P2P; the receiving node must reject the block.

**Acceptance Scenarios**:

1. **Given** a node receives a block from a peer containing well-formed stream entries, **When** the node validates the block, **Then** the block is accepted and appended to the chain.
2. **Given** a node receives a block containing an entry with missing stream name or key, **When** the node validates the block, **Then** the block is rejected.

---

### User Story 4 - Create and List Streams (Priority: P3)

As a blockchain client, I want to create named streams explicitly or have them auto-created on first publish, and list available streams, so that I can organize data flexibly by topic or application domain.

**Why this priority**: Stream management enables multi-tenant and multi-purpose use of the chain. Without it, all data is unorganized.

**Independent Test**: Can be tested by creating a stream explicitly, publishing to a non-existent stream (triggering auto-creation), listing all streams, and verifying both appear.

**Acceptance Scenarios**:

1. **Given** no stream named "inventory" exists, **When** a client creates stream "inventory" explicitly, **Then** the stream is recorded on the chain and available for publishing.
2. **Given** no stream named "logs" exists, **When** a client publishes an entry to stream "logs", **Then** the stream is auto-created and the entry is stored.
3. **Given** multiple streams exist, **When** a client requests the list of streams, **Then** all stream names are returned.
4. **Given** a stream named "inventory" already exists, **When** a client attempts to create it explicitly again, **Then** the request is rejected with a duplicate stream error.

---

### User Story 5 - Configure Per-Node Stream Permissions (Priority: P3)

As a node operator, I want to configure which streams my node allows publishing to so that I can restrict write access on a per-node basis for private or controlled deployments.

**Why this priority**: Per-node permissions enable private networks and controlled data environments without requiring a global authentication system.

**Independent Test**: Can be tested by configuring a node to allow publishing only to stream "allowed", then attempting to publish to "allowed" (succeeds) and "blocked" (rejected).

**Acceptance Scenarios**:

1. **Given** a node is configured with an allowlist of streams ["assets", "inventory"], **When** a client publishes to stream "assets", **Then** the entry is accepted.
2. **Given** a node is configured with an allowlist of streams ["assets", "inventory"], **When** a client publishes to stream "logs", **Then** the entry is rejected with a permission error.
3. **Given** a node has no stream permission restrictions configured (default), **When** a client publishes to any stream, **Then** the entry is accepted (open by default).

---

### Edge Cases

- What happens when the data payload is empty? (Allowed — empty strings are valid and may be used as markers or tombstones.)
- What happens when multiple entries share the same stream + key? (All are stored — keys are not unique; multiple entries represent a history/append log for that key.)
- What happens when a stream name contains special characters? (Stream names are restricted to alphanumeric characters, hyphens, and underscores; max 256 characters.)
- What happens when the data exceeds a reasonable size? (Entries exceeding 128 MB are rejected before block inclusion.)
- What happens when a node with stream restrictions receives a block via P2P containing entries for a restricted stream? (The block is still accepted — per-node permissions apply only to local RPC publishing, not to P2P block acceptance. The chain must remain consistent across nodes regardless of local permission settings.)
- What happens when auto-creation is attempted for a stream name that violates naming rules? (Rejected — naming rules apply to both explicit and implicit stream creation.)

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST define a structured stream entry format containing: stream name, key, and data payload (opaque string).
- **FR-002**: The system MUST support named streams as logical groupings of related entries (similar to MultiChain streams).
- **FR-003**: The system MUST allow clients to create named streams explicitly via RPC or implicitly by publishing to a non-existent stream name.
- **FR-004**: The system MUST reject stream entries with missing or empty stream name or key.
- **FR-005**: The system MUST replace the existing `addBlock` RPC endpoint with a `publish` endpoint that accepts stream entry parameters (stream, key, data).
- **FR-006**: The system MUST include full stream entry details (stream, key, data) in block query responses (`getBlockByIndex`, `getBlocksByKeys`).
- **FR-007**: The system MUST support two query modes: (a) history mode — returning all entries for a stream/key in chain order, and (b) latest mode — returning only the most recent entry for a stream/key.
- **FR-008**: The system MUST serialize and deserialize stream entries as part of the block's Boost.Serialization archive for persistence and P2P transfer.
- **FR-010**: The system MUST validate stream entry structure in blocks received via P2P before accepting the block into the chain.
- **FR-011**: The system MUST enforce naming rules on stream names (alphanumeric, hyphens, underscores; max 256 characters).
- **FR-012**: The system MUST prevent duplicate explicit stream creation (stream names are unique; implicit creation for an existing stream simply publishes to it).
- **FR-013**: The system MUST support per-node stream permissions configurable by the node operator, controlling which streams accept local RPC publish requests.
- **FR-014**: Per-node stream permissions MUST default to open (all streams allowed) when no restrictions are configured.
- **FR-015**: Per-node stream permissions MUST NOT affect P2P block acceptance — blocks from peers are validated for structure only, not against local permission rules.
- **FR-016**: The system MUST enforce a maximum entry data size of 128 MB.

### Key Entities

- **Stream**: A named channel for grouping related data entries. Created explicitly by clients or implicitly on first publish. Stream names are unique. Streams carry no metadata at creation time (bare name only).
- **Stream Entry**: A single data record published to a stream. Contains a stream name, a user-provided key, and a data payload (opaque string). Multiple entries may share the same key (append-log semantics). Maximum data size: 128 MB. Binary data is the client's responsibility to base64-encode before submission and decode on retrieval.
- **Block** (updated): A container for one or more stream entries, linked to the previous block. The legacy `data` field is removed; blocks contain only stream entries.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All new blocks contain structured stream entries with stream name, key, and data fields.
- **SC-002**: 100% of stream entries with missing stream name or key are rejected before block inclusion.
- **SC-003**: Blocks received via P2P with malformed stream entries are rejected by receiving nodes.
- **SC-004**: Clients can publish, query, and manage streams using only the RPC interface without direct chain access.
- **SC-005**: Queries by stream name + key return results in both history and latest modes without requiring a full chain scan.

## Assumptions

- Streams are open by default — any client can publish to any stream unless the node operator configures per-node stream permissions to restrict access.
- Stream entries are append-only. Entries cannot be modified or deleted once written to the chain. A newer entry with the same key represents an update; the full history is preserved.
- There are no existing blockchains to maintain backward compatibility with. The Block struct is modified in place with no serialization versioning.
- The consensus mechanism (PoW) and block validation rules from spec 002 remain unchanged; stream entry validation is an additional check layered on top.
- Digital signatures for stream entries are not included in this feature. Authentication/authorization for publishing is deferred to a future feature.
- Data payloads are stored as opaque strings; the node does not interpret, parse, or validate the content. Binary data support is a client-side concern (base64-encode before publishing, decode after retrieval).
