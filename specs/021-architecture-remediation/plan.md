# Implementation Plan: Architecture Remediation

**Branch**: `021-architecture-remediation` | **Date**: 2026-04-13 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/021-architecture-remediation/spec.md`

## Summary

Refactor the blockchain architecture to address audit §6 (Architecture Concerns) and §4.5 (replaceChain memory). Four workstreams: (1) widen `IChainReader` with four query methods and narrow consumer dependencies, (2) introduce `ChainService` as a mediator between network and domain layers, (3) standardize error handling on a `ChainError` exception hierarchy, (4) convert `replaceChain()` to streaming chunk-at-a-time processing with bounded memory and crash-safe rollback.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`)
**Storage**: Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`), keys (`keys.dat`), streams (`streams.dat`, `stream_index.dat`)
**Testing**: Catch2 (17 test binaries, ~7,676 lines)
**Target Platform**: Linux, macOS, Windows (cross-platform per Constitution §VII)
**Project Type**: CLI/daemon
**Performance Goals**: Low-latency query and response; 100-block chunk architecture for efficient block lookup
**Constraints**: Bounded memory for `replaceChain()` — peak memory proportional to batch size (100 blocks), not total chain length
**Scale/Scope**: Small private network; all nodes updated together; ~6,179 lines in `src/`, 16 `.cpp` files, 30 `.hpp` files

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Pre-Design | Post-Design | Notes |
|-----------|-----------|-------------|-------|
| I. C++20 | ✅ PASS | ✅ PASS | No new language features required |
| II. Build System (Autotools, -j8) | ✅ PASS | ✅ PASS | New files added to `src/Makefile.am` and `tests/Makefile.am` |
| III. Full Test Coverage | ✅ PASS | ✅ PASS | New `chain_service_tests` binary; existing tests updated |
| IV. Code Style | ✅ PASS | ✅ PASS | Follows existing conventions |
| V. Minimal Dependencies | ✅ PASS | ✅ PASS | No new external dependencies |
| VI. Mandatory TLS | ✅ PASS | ✅ PASS | No TLS changes |
| VII. Cross-Platform | ✅ PASS | ✅ PASS | No platform-specific code introduced |
| VIII. Feature Branches | ✅ PASS | ✅ PASS | On branch `021-architecture-remediation` |
| IX. Pre-1.0 API | ✅ PASS | ✅ PASS | Wire format changes allowed; all nodes update together |
| X. Low-Latency | ✅ PASS | ✅ PASS | Interface narrowing has zero runtime overhead; ChainService adds one function-call layer |
| XI. MIT License | ✅ PASS | ✅ PASS | All new code MIT-compatible |
| XII. .gitignore | ✅ PASS | ✅ PASS | New test binary `tests/chain_service_tests` added |
| XIII. Roadmap | ✅ PASS | ✅ PASS | `docs/ROADMAP.md` updated on completion |

**Gate result**: All 13 principles PASS. No violations.

## Project Structure

### Documentation (this feature)

```text
specs/021-architecture-remediation/
├── plan.md              # This file
├── spec.md              # Feature specification
├── research.md          # Phase 0: design decisions and rationale
├── data-model.md        # Phase 1: entity definitions and state transitions
├── quickstart.md        # Phase 1: build/test/overview
├── contracts/           # Phase 1: interface contracts
│   ├── IChainReader.md
│   ├── ChainService.md
│   ├── SyncMessages.md
│   └── ChainError.md
├── checklists/
│   └── requirements.md
└── tasks.md             # Phase 2 output (NOT created by /speckit.plan)
```

### Source Code (repository root)

```text
src/
├── ChainError.hpp          # NEW — exception hierarchy
├── ChainService.hpp        # NEW — service layer header
├── ChainService.cpp        # NEW — service layer implementation
├── IChainReader.hpp        # MODIFIED — 4 query methods added
├── IChainWriter.hpp        # MODIFIED — exception contract
├── IBlockchain.hpp         # MODIFIED — 4 query methods removed (moved to IChainReader)
├── Blockchain.hpp          # MODIFIED — override declarations updated
├── Blockchain.cpp          # MODIFIED — streaming replaceChain, ChainError exceptions
├── ChainPersistence.cpp    # MODIFIED — PersistenceError instead of log-and-continue
├── PeerManager.hpp         # MODIFIED — PeerError instead of bool returns
├── PeerManager.cpp         # MODIFIED — exception-based error handling
├── BlockPropagation.hpp    # MODIFIED — takes ChainService& instead of IBlockchain&
├── BlockPropagation.cpp    # MODIFIED — uses submitBlock() instead of direct append/save
├── main.cpp                # MODIFIED — wires ChainService into component graph
├── network/
│   ├── SyncMessages.hpp    # MODIFIED — chunk_index → start_index
│   ├── RpcServer.hpp       # MODIFIED — narrower interface dependencies
│   ├── RpcServer.cpp       # MODIFIED — uses IChainReader + IChainWriter
│   ├── PeerServer.hpp      # MODIFIED — uses ChainService, no chunkSize refs
│   ├── PeerServer.cpp      # MODIFIED — batches by block index, not chunk identity
│   ├── PeerClient.hpp      # MODIFIED — takes ChainService& for mutations
│   ├── PeerClient.cpp      # MODIFIED — uses submitSyncBatch()
│   └── SessionHandler.hpp  # MODIFIED — template for narrower types

tests/
├── chain_service_tests.cpp # NEW — ChainService unit tests
├── Makefile.am             # MODIFIED — add chain_service_tests target
└── [existing test files]   # MODIFIED — updated for new exception types
```

**Structure Decision**: Single-project layout following existing `src/` + `tests/` convention. New files placed at the same level as the classes they relate to. No new directories.

**Branch**: `[###-feature-name]` | **Date**: [DATE] | **Spec**: [link]
**Input**: Feature specification from `/specs/[###-feature-name]/spec.md`

**Note**: This template is filled in by the `/speckit.plan` command. See `.specify/templates/plan-template.md` for the execution workflow.

## Summary

[Extract from feature spec: primary requirement + technical approach from research]

## Technical Context

<!--
  ACTION REQUIRED: Replace the content in this section with the technical details
  for the project. The structure here is presented in advisory capacity to guide
  the iteration process.
-->

**Language/Version**: [e.g., Python 3.11, Swift 5.9, Rust 1.75 or NEEDS CLARIFICATION]  
**Primary Dependencies**: [e.g., FastAPI, UIKit, LLVM or NEEDS CLARIFICATION]  
**Storage**: [if applicable, e.g., PostgreSQL, CoreData, files or N/A]  
**Testing**: [e.g., pytest, XCTest, cargo test or NEEDS CLARIFICATION]  
**Target Platform**: [e.g., Linux server, iOS 15+, WASM or NEEDS CLARIFICATION]
**Project Type**: [e.g., library/cli/web-service/mobile-app/compiler/desktop-app or NEEDS CLARIFICATION]  
**Performance Goals**: [domain-specific, e.g., 1000 req/s, 10k lines/sec, 60 fps or NEEDS CLARIFICATION]  
**Constraints**: [domain-specific, e.g., <200ms p95, <100MB memory, offline-capable or NEEDS CLARIFICATION]  
**Scale/Scope**: [domain-specific, e.g., 10k users, 1M LOC, 50 screens or NEEDS CLARIFICATION]

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

[Gates determined based on constitution file]

## Project Structure

### Documentation (this feature)

```text
specs/[###-feature]/
├── plan.md              # This file (/speckit.plan command output)
├── research.md          # Phase 0 output (/speckit.plan command)
├── data-model.md        # Phase 1 output (/speckit.plan command)
├── quickstart.md        # Phase 1 output (/speckit.plan command)
├── contracts/           # Phase 1 output (/speckit.plan command)
└── tasks.md             # Phase 2 output (/speckit.tasks command - NOT created by /speckit.plan)
```

### Source Code (repository root)
<!--
  ACTION REQUIRED: Replace the placeholder tree below with the concrete layout
  for this feature. Delete unused options and expand the chosen structure with
  real paths (e.g., apps/admin, packages/something). The delivered plan must
  not include Option labels.
-->

```text
# [REMOVE IF UNUSED] Option 1: Single project (DEFAULT)
src/
├── models/
├── services/
├── cli/
└── lib/

tests/
├── contract/
├── integration/
└── unit/

# [REMOVE IF UNUSED] Option 2: Web application (when "frontend" + "backend" detected)
backend/
├── src/
│   ├── models/
│   ├── services/
│   └── api/
└── tests/

frontend/
├── src/
│   ├── components/
│   ├── pages/
│   └── services/
└── tests/

# [REMOVE IF UNUSED] Option 3: Mobile + API (when "iOS/Android" detected)
api/
└── [same as backend above]

ios/ or android/
└── [platform-specific structure: feature modules, UI flows, platform tests]
```

**Structure Decision**: [Document the selected structure and reference the real
directories captured above]

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., 4th project] | [current need] | [why 3 projects insufficient] |
| [e.g., Repository pattern] | [specific problem] | [why direct DB access insufficient] |
