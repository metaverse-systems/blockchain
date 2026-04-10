# Research: 006 — Transaction Model (Stream-Based Key/Value Store)

**Date**: 2026-04-10  
**Feature**: [spec.md](spec.md)

## R-001: Stream Entry Serialization with Boost.Serialization

**Decision**: Add a `StreamEntry` struct with Boost.Serialization support, stored as a `std::vector<StreamEntry>` within `Block`. The existing `data` field is removed and replaced by `entries`.

**Rationale**: The project already uses Boost.Serialization for all persistence and P2P wire format. Since there are no existing blockchains to maintain compatibility with, the `data` field can be cleanly replaced by the structured `entries` vector. No serialization versioning is needed — the Block struct remains at version 0 with the updated field layout.

**Alternatives considered**:
- Encoding stream entries as JSON in the existing `data` string: Simpler but loses type safety, imposes JSON parsing overhead on every block operation, and doesn't integrate with the existing binary serialization pipeline.
- Keeping `data` alongside `entries` for backward compatibility: Unnecessary since no existing chains exist.

## R-002: No Backward Compatibility Needed

**Decision**: No serialization versioning or backward compatibility layer. The Block struct is modified in place at version 0. The `data` field is replaced by `std::vector<StreamEntry> entries`.

**Rationale**: There are no existing blockchains in production. No chain data needs to be preserved across this change. This eliminates the complexity of `BOOST_CLASS_VERSION`, version checks in the serialize template, and dual-format handling throughout the codebase.

**Alternatives considered**:
- Boost.Serialization versioning (version 0 vs 1): Adds unnecessary complexity when no legacy data exists.

## R-003: Stream Index Data Structure

**Decision**: Extend the existing `keyIndexMap` pattern. Add a new in-memory index: `std::map<std::string, std::map<std::string, std::vector<size_t>>>` mapping `stream → key → [block indices]`. Additionally, maintain a `std::set<std::string>` of known stream names. Both are persisted via Boost.Serialization to the blockchain data directory.

**Rationale**: The existing `keyIndexMap` (`std::map<std::string, std::vector<size_t>>`) already provides efficient key-to-block-index lookup. The stream index follows the same pattern, adding one more level of hierarchy. The stream name set supports `listStreams` and duplicate-creation checks. Persistence follows the same binary archive pattern as `keys.dat`.

**Alternatives considered**:
- Flat composite key (`"stream:key"`): Loses ability to query all entries in a stream without scanning all composite keys.
- SQLite-backed index: Adds a new dependency (forbidden by constitution §V without approval).

## R-004: Per-Node Stream Permissions via config.json

**Decision**: Add a `streams` section to `config.json` with an `allowed_streams` array. Empty array (default) means all streams are allowed. Non-empty array restricts local RPC publishing to listed streams only. Checked only in the RPC handler, never during P2P block acceptance.

**Rationale**: Follows the existing `NodeConfig` pattern for configuration. Using `config.json` (not a separate file) keeps configuration centralized. The permission check is purely local — it does not affect chain consensus or P2P validation, maintaining consistency across nodes.

**Alternatives considered**:
- Separate permissions file: Adds file management complexity.
- Deny-list model: Allow-list is simpler and more secure by default.

## R-005: RPC Endpoint Design for Streams

**Decision**: Replace the existing `addBlock` method with `publish` as the primary way to add data. Add new methods: `createStream`, `listStreams`, `getStreamEntries` (history mode), `getStreamEntry` (latest mode). The legacy `addBlock` method is removed since there are no existing clients to maintain compatibility with.

**Rationale**: A clean break is possible since there are no existing blockchains or clients. The `publish` method with stream/key/data parameters is the sole way to add blocks. This avoids maintaining two overlapping code paths. New query methods follow the existing RPC naming and parameter conventions. Two separate query methods (history vs latest) are cleaner than a mode parameter on a single method.

**Alternatives considered**:
- Keeping `addBlock` alongside `publish`: Creates two overlapping methods with no users of the old one.
- Single `getStreamEntries` with a `latest` boolean param: Viable but less explicit.

## R-006: 128 MB Entry Size Enforcement

**Decision**: Validate entry data size in the RPC handler before block creation, and in the P2P block validation path before appending. Check `entry.data.size() <= 128 * 1024 * 1024`.

**Rationale**: Size validation at both ingress points (RPC and P2P) prevents oversized entries from entering the chain. The 128 MB limit is checked as a simple string length comparison — no streaming or chunking needed since Boost.Serialization already handles the full payload in memory.

**Alternatives considered**:
- Configurable per-node limit: Adds consensus disagreement risk if nodes have different limits. A fixed protocol-level limit is safer.
- Streaming large entries across multiple blocks: Significantly more complex and not requested.
