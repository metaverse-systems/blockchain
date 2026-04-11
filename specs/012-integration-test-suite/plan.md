# Implementation Plan: Integration Test Suite

**Branch**: `012-integration-test-suite` | **Date**: 2026-04-11 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/012-integration-test-suite/spec.md`

## Summary

Add end-to-end integration tests that exercise all JSON-RPC endpoints and two-node P2P sync over real TLS connections. Tests run blockchain nodes in-process on separate threads with self-signed certificates generated at test time, dynamically assigned ports, and RAII-based cleanup.

## Technical Context

**Language/Version**: C++20 (`-std=c++20`)
**Primary Dependencies**: Boost (Asio, Serialization), OpenSSL (EVP SHA-256, X.509 cert generation), nlohmann/json (vendored `src/json.hpp`), Catch2 (test framework)
**Storage**: Boost.Serialization binary chunk files in temporary directories (cleaned per test)
**Testing**: Catch2 — same framework used by all existing test binaries
**Target Platform**: Linux, macOS, Windows (per Constitution §VII)
**Project Type**: C++ blockchain library/daemon with RPC and P2P interfaces
**Performance Goals**: Each integration test completes within 60 seconds; P2P sync of a 10-block chain within 30 seconds (SC-003, SC-006)
**Constraints**: In-process threading model (clarified in spec); no new external dependencies (Constitution §V); TLS mandatory on all network interfaces (Constitution §VI)
**Scale/Scope**: ~2 new test binaries, ~20–25 test cases total

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| # | Principle | Status | Notes |
|---|-----------|--------|-------|
| I | C++20 standard | PASS | All new test code uses `-std=c++20` matching existing test flags |
| II | GNU Autotools build system | PASS | New test binaries registered in `tests/Makefile.am`; `make -j8` |
| III | Full test coverage | PASS | This feature IS the integration test suite; tests are runnable individually |
| IV | Code style | PASS | Follow existing conventions (`#pragma once`, naming, braces) |
| V | Minimal dependencies | PASS | No new dependencies; OpenSSL programmatic cert generation uses existing dep |
| VI | Mandatory TLS | PASS | All test connections use real TLS with self-signed certs |
| VII | Cross-platform support | PASS | Boost.Asio port-0 binding + OpenSSL cert API work on all three platforms |
| VIII | Feature branches with PRs | PASS | Already on `012-integration-test-suite` branch |
| IX | Pre-1.0 API stability | N/A | Tests consume existing interfaces, no protocol changes |
| X | Low-latency performance | N/A | Test-only code, no production hot paths |
| XI | MIT license | PASS | New test files under MIT |
| XII | .gitignore maintenance | PASS | New test binaries will be added to `.gitignore` |
| XIII | Roadmap currency | PASS | `docs/ROADMAP.md` will be updated on completion |

**Gate result**: ALL PASS — proceed to Phase 0.

## Project Structure

### Documentation (this feature)

```text
specs/012-integration-test-suite/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md        # Phase 1 output
├── quickstart.md        # Phase 1 output
├── contracts/           # Phase 1 output
└── tasks.md             # Phase 2 output (/speckit.tasks)
```

### Source Code (repository root)

```text
tests/
├── rpc_integration_tests.cpp       # NEW — full RPC endpoint tests over real TLS
├── p2p_sync_integration_tests.cpp  # NEW — two-node P2P sync and propagation tests
├── IntegrationTestFixture.hpp      # NEW — shared node lifecycle, TLS cert gen, port allocation
├── Makefile.am                     # MODIFIED — add two new test binaries
src/
├── (no source modifications expected)
.gitignore                           # MODIFIED — add new test binary names
```

**Structure Decision**: New test files live alongside existing tests in `tests/`. A shared header (`IntegrationTestFixture.hpp`) provides the in-process node fixture reused by both test binaries. This mirrors the existing pattern where `MockBlockchain.hpp` is shared across test binaries.
# Implementation Plan: [FEATURE]

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
