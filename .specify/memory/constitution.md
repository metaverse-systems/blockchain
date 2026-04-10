<!--
Sync Impact Report
==================
Version change: 0.0.0 → 1.0.0
Bump rationale: MAJOR — initial constitution ratification, all
principles defined for the first time.

Modified principles: N/A (initial version)

Added sections:
  - Core Principles (I–XI)
  - Forbidden Actions
  - Development Workflow
  - Governance

Removed sections: N/A

Templates requiring updates:
  - .specify/templates/plan-template.md         ✅ compatible (Constitution Check is dynamic)
  - .specify/templates/spec-template.md         ✅ compatible (no constitution-specific tokens)
  - .specify/templates/tasks-template.md        ✅ compatible (no constitution-specific tokens)
  - .specify/templates/checklist-template.md    ✅ compatible

Follow-up TODOs: none
-->

# metaverse-systems/blockchain Constitution

A C++ blockchain library for storing and replicating data in a
tamper-resistant way.

## Core Principles

### I. Language Standard

All code MUST compile under `-std=c++20`. C++20 is the target
standard. Features from later standards MUST NOT be used unless
the project formally adopts them via a constitution amendment.

### II. Build System

GNU Autotools is the sole build system. All build definitions
MUST use `configure.ac`, `Makefile.am`, and the Autotools
toolchain. Migration to another build system is prohibited
without a constitution amendment.

### III. Full Test Coverage

Every new feature MUST include both unit tests and network
integration tests. The test framework is Catch2. Mock objects
(`MockChunk`, `MockSessionHandler`, `MockAcceptor`) MUST be
used to isolate units under test. Test binaries MUST be
runnable via `make check`.

### IV. Code Style

No formal style guide is enforced. All new code MUST follow
the conventions already present in the codebase (naming,
indentation, brace placement, header guards via `#pragma once`).

### V. Minimal Dependencies

External dependencies MUST be minimized. The approved set is:

- Boost (Asio, Serialization)
- OpenSSL
- nlohmann/json (vendored in `src/json.hpp`)
- Catch2 (test only)

Adding a new dependency requires explicit approval and a
documented justification.

### VI. Mandatory TLS (NON-NEGOTIABLE)

SSL/TLS MUST be used for all network communication — both the
JSON-RPC interface and the P2P binary protocol. Removing or
weakening TLS protections on any network interface is
**strictly forbidden**.

### VII. Cross-Platform Support

The project MUST build and run on Linux, macOS, and Windows.
Platform-specific code MUST be guarded with appropriate
preprocessor checks and covered by tests on all three targets.

### VIII. Feature Branches with Pull Requests

All changes MUST be developed on feature branches and merged
via pull requests. Direct commits to `main` are prohibited.

### IX. Pre-1.0 API Stability

The project is pre-1.0 and actively evolving. The JSON-RPC and
P2P protocols MAY change freely without backward compatibility
guarantees. This policy will be revisited before a 1.0 release.

### X. Low-Latency Performance

The system MUST be optimized for low-latency query and
response. The chunk-based architecture (100 blocks per chunk)
MUST support efficient block lookup and retrieval. Performance
regressions in hot paths MUST be justified.

### XI. MIT License

All source code is released under the MIT license. Every new
source file MUST be compatible with this license. Third-party
code MUST carry a compatible license.

## Forbidden Actions

The following actions are **unconditionally prohibited**:

- Removing or weakening SSL/TLS on any network interface.
- Committing directly to the `main` branch.
- Adding unapproved external dependencies.
- Introducing code that does not compile under `-std=c++20`.

## Development Workflow

1. Create a feature branch from `main`.
2. Implement changes with full unit and integration tests.
3. Ensure `make check` passes locally.
4. Open a pull request for review.
5. Merge only after review approval and passing CI.

## Governance

This constitution supersedes all other project practices.
Amendments require:

1. A pull request modifying this file with a clear rationale.
2. Review and approval by a project maintainer.
3. A version bump following semantic versioning:
   - **MAJOR**: Principle removal or incompatible redefinition.
   - **MINOR**: New principle or materially expanded guidance.
   - **PATCH**: Clarifications, wording, or typo fixes.
4. Update of `LAST_AMENDED_DATE` to the amendment date.

All pull requests and code reviews MUST verify compliance with
these principles.

**Version**: 1.0.0 | **Ratified**: 2026-04-10 | **Last Amended**: 2026-04-10
