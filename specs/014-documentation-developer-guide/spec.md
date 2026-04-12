# Feature Specification: Documentation & Developer Guide

**Feature Branch**: `014-documentation-developer-guide`  
**Created**: 2026-04-12  
**Status**: Draft  
**Input**: User description: "Implement 014 — Documentation & Developer Guide. README.md contains a single heading. The quickstart and contract docs exist in the spec directory but are not surfaced to developers. Write user-facing documentation."

## Clarifications

### Session 2026-04-12

- Q: How should curl examples handle RPC TLS? → A: Use `--cacert ca.pem` in all examples (assumes self-signed CA from quickstart TLS setup)
- Q: Should the API reference use exact wire-format method names or friendly aliases? → A: Use exact wire-format method names only (e.g., `publish`, `getStreamEntry`, `getStreamEntries`)
- Q: What diagram format for the architecture overview? → A: Mermaid diagrams embedded in Markdown (rendered natively by GitHub)
- Q: Should the quickstart cover single-node or multi-node setup? → A: Full multi-node quickstart (two nodes, peer connection, block propagation demo)
- Q: How should RPC methods be organized in the API reference? → A: Group by functional domain (Streams, Blocks, Peers, Node, Merkle, Sync) with a table of contents

## User Scenarios & Testing *(mandatory)*

### User Story 1 - New Developer Builds from Source (Priority: P1)

A new developer discovers the project, clones the repository, and wants to build the blockchain node from source. They open `README.md` and find clear build instructions covering all three supported platforms (Linux, macOS, Windows). They follow the steps and successfully compile and run the node.

**Why this priority**: Without build instructions, no one outside the original team can use the project. This is the minimum viable documentation.

**Independent Test**: Can be tested by following the README build steps on a clean machine with only the listed prerequisites installed, producing a working `blockchain` binary.

**Acceptance Scenarios**:

1. **Given** a developer on Linux with `g++`, Boost, and OpenSSL installed, **When** they follow the README build instructions, **Then** the project compiles successfully and produces the `blockchain` binary.
2. **Given** a developer on macOS with `clang`, Homebrew-installed Boost, and OpenSSL, **When** they follow the README build instructions, **Then** the project compiles successfully.
3. **Given** a developer on Windows with MSYS2 and the listed packages, **When** they follow the README build instructions, **Then** the project compiles successfully.
4. **Given** a developer who has built the binary, **When** they follow the quickstart section, **Then** they can initialize two blockchain directories, generate configs and TLS certificates, start two nodes, connect them as peers, and observe a block propagating from one node to the other.

---

### User Story 2 - Operator Configures and Runs a Node (Priority: P2)

An operator wants to deploy and configure a blockchain node. They read the configuration guide to understand all available settings (`config.json` fields, CLI flags, TLS setup, peer configuration). They configure the node for their environment and start it.

**Why this priority**: Operators need to know how to configure the node after building it. This is essential for anyone running the software in any environment.

**Independent Test**: Can be tested by using only the documented configuration reference to set up a node with custom RPC port, P2P port, log level, and seed peers, then verifying the node starts with those settings.

**Acceptance Scenarios**:

1. **Given** an operator reading the configuration guide, **When** they look up any CLI flag, **Then** they find its name, description, default value, and an example.
2. **Given** an operator reading the configuration guide, **When** they look up any `config.json` field, **Then** they find its key path, type, default value, and description.
3. **Given** an operator who needs TLS certificates for P2P communication, **When** they read the TLS setup section, **Then** they find step-by-step instructions for generating or providing certificates.

---

### User Story 3 - Developer Integrates via RPC API (Priority: P3)

A developer wants to build a client application that interacts with the blockchain node's JSON-RPC API. They read the API reference to discover available methods, their parameters, and response formats. They use this documentation to make successful RPC calls.

**Why this priority**: The RPC API is the primary programmatic interface to the node. Without reference documentation, external developers cannot build on top of it.

**Independent Test**: Can be tested by using only the documented RPC examples to make a successful call to every listed endpoint and verifying the response matches the documented format.

**Acceptance Scenarios**:

1. **Given** a developer reading the RPC API reference, **When** they look up any RPC method, **Then** they find the method name, parameter schema, response schema, and at least one `curl` example.
2. **Given** a developer sending a request matching a documented example, **When** the node is running, **Then** the response format matches the documented response schema.
3. **Given** a developer looking for error handling guidance, **When** they read the API reference, **Then** they find documented error codes and their meanings.

---

### User Story 4 - Contributor Understands Architecture (Priority: P4)

A potential contributor wants to understand the high-level architecture before making changes. They read the architecture overview to learn about the major components (consensus, P2P, persistence, RPC), how they interact, and the data flow through the system.

**Why this priority**: Valuable for growing the contributor base, but not blocking for users or operators.

**Independent Test**: Can be tested by asking a developer unfamiliar with the codebase to read the architecture overview and then correctly describe the role of each major component and the data flow for mining and propagating a block.

**Acceptance Scenarios**:

1. **Given** a contributor reading the architecture overview, **When** they look for the major subsystems, **Then** they find descriptions of the consensus engine, P2P networking, persistence layer, and RPC server.
2. **Given** a contributor reading the architecture overview, **When** they look for data flow, **Then** they find a description of how blocks are created, validated, persisted, and propagated to peers.

---

### User Story 5 - Contributor Submits a Change (Priority: P5)

A contributor wants to submit a code change. They read the contributing guide to understand the build process, how to run tests, coding conventions, and the pull request workflow.

**Why this priority**: Important for community growth but lowest priority since the project must be usable first.

**Independent Test**: Can be tested by following the contributing guide to make a trivial change, run the test suite, and verify all expected steps are documented.

**Acceptance Scenarios**:

1. **Given** a contributor reading the contributing guide, **When** they look for how to run tests, **Then** they find the exact commands to build and execute the test suite.
2. **Given** a contributor reading the contributing guide, **When** they look for coding conventions, **Then** they find the C++ standard, formatting expectations, and commit message guidelines.

---

### Edge Cases

- What happens when a user follows build instructions but has an unsupported compiler version? The prerequisites section must list minimum versions.
- What happens when documented RPC examples reference features from a newer version? Each documentation page should note the version or spec that introduced a feature.
- What happens when a config field is deprecated or renamed in a future version? The configuration guide should note the version each field was introduced.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project README MUST contain a project description explaining what the blockchain node does and its key capabilities.
- **FR-002**: The README MUST contain build prerequisites listing required compilers (with minimum versions), libraries (Boost, OpenSSL), and build tools for Linux, macOS, and Windows.
- **FR-003**: The README MUST contain step-by-step build instructions for all three supported platforms (Linux, macOS, Windows/MSYS2).
- **FR-004**: The README MUST contain a quickstart section that walks a user from first build to a running two-node network (directory creation, config generation, TLS certificate generation, starting two nodes, connecting them as peers, and demonstrating block propagation).
- **FR-005**: A configuration reference document MUST list every CLI flag with its name, description, type, default value, and an example.
- **FR-006**: The configuration reference MUST list every `config.json` field with its JSON key path, type, default value, and description.
- **FR-007**: The documentation MUST include TLS setup instructions explaining how to generate or provide the required certificate, key, and CA files for P2P mutual TLS.
- **FR-008**: An RPC API reference document MUST list every JSON-RPC method with its name, description, parameter schema, response schema, and at least one `curl` example using `--cacert ca.pem` for TLS verification (consistent with the quickstart TLS setup). Methods MUST be grouped by functional domain (Streams, Blocks, Peers, Node, Merkle, Sync) with a table of contents.
- **FR-009**: The RPC API reference MUST document error responses including standard JSON-RPC error codes used by the node.
- **FR-010**: An architecture overview document MUST describe the major subsystems (consensus, P2P networking, persistence, RPC) and their interactions, using Mermaid diagrams for visual representation.
- **FR-011**: The architecture overview MUST describe the data flow for key operations (block mining, block propagation, chain sync, stream publish) with at least one Mermaid sequence or flowchart diagram.
- **FR-012**: A contributing guide MUST document how to build the project, run the test suite, coding conventions (C++20 standard, style expectations), and the pull request workflow.
- **FR-013**: The README MUST link to all other documentation files so they are discoverable from the repository landing page.
- **FR-014**: All documentation MUST be written in Markdown and stored in the repository (README.md at root, additional docs in `docs/`).

### Key Entities

- **README.md**: Root-level entry point for the project. Contains project overview, build instructions, quickstart, and links to detailed docs.
- **docs/configuration.md**: Comprehensive CLI and config.json reference.
- **docs/rpc-api.md**: JSON-RPC method reference with examples.
- **docs/architecture.md**: High-level system architecture and data flow.
- **docs/contributing.md**: Contributor guide with build, test, style, and PR instructions.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A developer with the listed prerequisites can go from `git clone` to a running node in under 15 minutes by following only the README.
- **SC-002**: 100% of CLI flags and `config.json` fields are documented in the configuration reference with name, type, default, and description.
- **SC-003**: 100% of JSON-RPC methods are documented in the API reference with parameters, response format, and a working example.
- **SC-004**: Every documentation file is reachable from README.md within one click (direct link).
- **SC-005**: A developer unfamiliar with the codebase can correctly identify the four major subsystems and describe block propagation flow after reading only the architecture overview.
- **SC-006**: A contributor can build the project and run the full test suite by following only the contributing guide.

## Assumptions

- The target audience for the README and quickstart is developers comfortable with command-line tools and building C++ projects from source.
- The three supported platforms are Linux (gcc/clang), macOS (clang/Homebrew), and Windows (MSYS2), consistent with the CI pipeline (spec 013).
- The documentation describes the current state of the software (through spec 013). Future features (015–017) will update documentation as part of their own specs.
- The existing `ROADMAP.md` in `docs/` will remain as-is; this spec does not modify it.
- Documentation will be written in standard GitHub-Flavored Markdown with no external documentation tooling or static site generators required.
- The RPC API reference documents all 20 methods implemented through spec 010, using their exact wire-format names: `publish`, `createStream`, `listStreams`, `getStreamEntries`, `getStreamEntry`, `requestSync`, `getBlockByIndex`, `getBlocksByKeys`, `addPeer`, `removePeer`, `listPeers`, `banPeer`, `unbanPeer`, `getInclusionProof`, `verifyInclusionProof`, `getBlockHeader`, `getNodeStatus`, `getBlockRange`, `getChainLength`, `getChunkCount`.
- Configuration reference covers the config.json schema as implemented through spec 009 and the CLI flags from CliParser.
