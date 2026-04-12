# Feature Specification: Compile-Time Optimization

**Feature Branch**: `015-compile-time-optimization`  
**Created**: 2026-04-12  
**Status**: Draft  
**Input**: User description: "`time make -j8 check TESTS=` takes 13 minutes, 20 seconds. Look for ways to speed up compile time."

## Clarifications

### Session 2026-04-12

- Q: Which compile flags should the shared core objects use? → A: Option B — base flags (`-std=c++20 -Wall -Wextra -pedantic $(BOOST_CPPFLAGS) ${OPENSSL_CFLAGS}`) for shared objects (OpenSSL CFLAGS required for header includes); the main binary adds `-O3` independently
- Q: Should `blockchain_tests` (9 test files in one binary) be split into separate binaries? → A: No — keep as a single monolithic binary; test organization stays unchanged
- Q: Should shared core objects use a static archive or libtool convenience library? → A: Plain static archive (`noinst_LIBRARIES`) — no libtool dependency
- Q: Should SC-001 success threshold (50% reduction) be more aggressive given ~84% compile-unit reduction? → A: Keep 50% as the minimum threshold — conservative but achievable; actual results likely exceed it

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Developer Builds Full Project Faster (Priority: P1)

As a developer, I want the full build (main binary + all test binaries) to complete significantly faster so that my development feedback loop is shorter and I spend less time waiting.

**Why this priority**: The 13-minute, 20-second compile time (`make -j8 check TESTS=`) directly impacts every developer on every code change. Reducing this is the highest-leverage improvement for developer productivity.

**Independent Test**: Run `time make -j8 check TESTS=` from a clean build state and verify the total wall-clock time is substantially reduced from the 13:20 baseline.

**Acceptance Scenarios**:

1. **Given** a clean build directory (after `make clean`), **When** I run `make -j8 check TESTS=`, **Then** the full build completes in meaningfully less time than the 13:20 baseline
2. **Given** the project sources are unchanged, **When** I run `make -j8` a second time, **Then** the build completes near-instantly (nothing to recompile)

---

### User Story 2 - Developer Rebuilds After Single-File Change Faster (Priority: P2)

As a developer, I want incremental builds after changing a single source file to be fast, so that the edit-compile-test cycle is responsive.

**Why this priority**: Incremental builds happen far more frequently than clean builds. Today, changing one source file may trigger redundant recompilation across many test binaries because each test binary independently compiles the same source files.

**Independent Test**: After a full build, touch a single `.cpp` file, then run `time make -j8` and verify only minimal recompilation occurs.

**Acceptance Scenarios**:

1. **Given** a fully built project, **When** I modify a single source file (e.g., `Block.cpp`) and run `make -j8`, **Then** the recompilation finishes in a small fraction of the clean-build time
2. **Given** a fully built project, **When** I modify a single test file (e.g., `block_tests.cpp`) and run `make -j8`, **Then** only the affected test binary is rebuilt

---

### User Story 3 - CI Pipeline Builds Complete Faster (Priority: P3)

As a project maintainer, I want CI builds to finish faster so that pull request feedback is timely and CI resource costs are lower.

**Why this priority**: CI builds run from clean state on every push/PR. The same 13+ minute compile time affects every pipeline run, consuming compute resources and delaying merge feedback.

**Independent Test**: Observe CI pipeline build duration before and after the optimization and verify a meaningful reduction.

**Acceptance Scenarios**:

1. **Given** a CI pipeline triggered by a code push, **When** the build step runs, **Then** it completes in meaningfully less time than the current baseline
2. **Given** the optimization is applied, **When** CI runs on multiple platforms, **Then** all platforms see improved build times (the improvement is not platform-specific)

---

### Edge Cases

- What happens when a header file shared across many translation units is modified? The rebuild should still be faster than the current baseline.
- What happens on a single-core machine (no parallelism)? Build time should still improve or at minimum not regress.
- What happens when a new source file or test binary is added? The build system structure should make it straightforward to add new targets without reintroducing redundant compilation.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The build system MUST eliminate redundant compilation of the same source files across multiple targets (the main binary and 13 test binaries currently each compile the same 11 core source files independently)
- **FR-002**: The build system MUST produce the same set of output binaries (1 main binary + 13 test binaries) with identical functionality
- **FR-003**: The build system MUST continue to support parallel builds (`make -j8` or similar)
- **FR-004**: The build system MUST continue to support incremental builds (only recompile what changed)
- **FR-005**: Shared core objects MUST be compiled with base flags (`-std=c++20 -Wall -Wextra -pedantic $(BOOST_CPPFLAGS) ${OPENSSL_CFLAGS}`); `${OPENSSL_CFLAGS}` is required because core sources (e.g., `network/*.cpp`) include OpenSSL headers. The main binary adds `-O3` to its own target-specific compilation, and test binaries use their existing flags at link time
- **FR-006**: Adding a new test binary MUST NOT require re-listing all core source files; it should only specify its own test source and link against the shared static archive
- **FR-007**: The build system MUST continue to work with the existing Autotools (Automake/Autoconf) toolchain

### Key Entities

- **Core source files**: The 11 `.cpp` files under `src/` that are currently compiled independently by every target (Block.cpp, Blockchain.cpp, Chunk.cpp, utils.cpp, NodeConfig.cpp, PeerManager.cpp, BlockPropagation.cpp, MerkleTree.cpp, network/RpcServer.cpp, network/PeerServer.cpp, network/PeerClient.cpp)
- **Test binaries**: The 13 `check_PROGRAMS` targets in `tests/Makefile.am`, each with their own test-specific source files
- **Main binary**: The `blockchain` program built from `src/Makefile.am`

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Clean compile time (`make -j8 check TESTS=` from scratch) is reduced by at least 50% from the 13:20 baseline
- **SC-002**: Incremental build time after a single core source file change is reduced proportionally (each changed source is compiled once, not once per target)
- **SC-003**: All existing tests pass with identical results after the build restructuring
- **SC-004**: The number of compilation units in a clean build is reduced from ~177 (11 core sources × 14 targets + 23 target-specific) to ~34 (11 shared + 23 target-specific)

## Assumptions

- The Autotools (Automake/Autoconf) build system will be retained; a migration to CMake, Meson, or another build system is out of scope
- The current `-std=c++20` standard, compiler flags, and linked libraries remain unchanged
- The optimization focuses on build structure (eliminating redundant compilation), not on code-level changes like precompiled headers or template instantiation optimization
- Hardware and compiler version are held constant for baseline comparisons
- The `json.hpp` header (vendored nlohmann/json) is a known heavy header but addressing it via precompiled headers is out of scope for this feature
