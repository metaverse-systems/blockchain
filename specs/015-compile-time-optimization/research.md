# Research: Compile-Time Optimization

**Feature**: 015-compile-time-optimization  
**Date**: 2026-04-12

## Research Task 1: Automake `noinst_LIBRARIES` for Shared Static Archives

### Decision
Use `noinst_LIBRARIES` with a dedicated subdirectory and `_SOURCES` / `_CXXFLAGS` to compile core objects once into a `.a` archive consumed by all targets.

### Rationale
- Automake natively supports `noinst_LIBRARIES` (no libtool needed) for static archives internal to the build
- Objects in the archive are compiled once under the archive's own `_CXXFLAGS`, then linked into each target's `_LDADD`
- This eliminates the per-target `_SOURCES` duplication that causes the redundant compilations
- `subdir-objects` in `AM_INIT_AUTOMAKE` (already enabled in `configure.ac`) makes cross-directory source references work correctly

### Alternatives Considered
- **Libtool convenience library (`noinst_LTLIBRARIES`)**: Adds libtool as a build dependency; unnecessary since all targets are statically linked executables. Rejected per clarification.
- **Manual `.o` listing with `EXTRA_xxx_DEPENDENCIES`**: Brittle; breaks incremental builds and doesn't play well with Automake's dependency tracking. Rejected.
- **Moving to CMake/Meson**: Out of scope per constitution (Principle II: GNU Autotools is the sole build system). Rejected.

## Research Task 2: Where to Place the Static Archive

### Decision
Build the static archive (`libblockchain_core.a`) in `src/Makefile.am` alongside the main binary. The `tests/Makefile.am` references it via a relative path (`../src/libblockchain_core.a`).

### Rationale
- The core sources already live under `src/`. Building the archive there keeps source files co-located with their compilation rules.
- The main binary (`blockchain`) can link `libblockchain_core.a` plus its own `main.cpp` and `CliParser.cpp` (which are main-binary-specific).
- Test targets in `tests/Makefile.am` add `../src/libblockchain_core.a` to their `_LDADD` and drop all `../src/*.cpp` from their `_SOURCES`.

### Alternatives Considered
- **Top-level `Makefile.am`**: Would require restructuring the SUBDIRS ordering and is non-idiomatic for Autotools. Rejected.
- **Separate `lib/` subdirectory**: Unnecessary code movement; the sources are already in `src/`. Rejected.

## Research Task 3: Flag Separation Strategy

### Decision
The static archive is compiled with base flags: `-std=c++20 -Wall -Wextra -pedantic $(BOOST_CPPFLAGS)`. The main binary adds `-O3 ${OPENSSL_CFLAGS}` only to its own `main.cpp` and `CliParser.cpp`. Test binaries add only their own `_CXXFLAGS` for their test `.cpp` files.

### Rationale
- Per clarification, shared objects use minimal base flags
- `-O3` on the archive would mean tests exercise optimized code which could mask undefined behavior during development
- `${OPENSSL_CFLAGS}` is needed for headers included by `network/*.cpp` files. Since those files are IN the archive, `${OPENSSL_CFLAGS}` must actually be part of the archive's flags (it provides include paths needed to compile, not just link). This is a correction: the archive MUST include `${OPENSSL_CFLAGS}` because `network/RpcServer.cpp` and others `#include <openssl/...>`.

### Refinement
After analysis, the archive flags must be: `-std=c++20 -Wall -Wextra -pedantic $(BOOST_CPPFLAGS) ${OPENSSL_CFLAGS}` — the OpenSSL CFLAGS provide necessary include paths for compilation, not optimization. The `-O3` is the only flag that should differ between archive and main binary.

### Alternatives Considered
- **Compile archive twice (optimized + debug)**: Doubles build work and negates much of the benefit. Rejected per clarification.

## Research Task 4: `CliParser.cpp` Placement

### Decision
`CliParser.cpp` stays out of the static archive and remains compiled only by the main binary and `cli_tests`.

### Rationale
- `CliParser.cpp` depends on `boost::program_options` which is only linked by the main binary and `cli_tests`
- Including it in the shared archive would force all test binaries to link `BOOST_PROGRAM_OPTIONS_LIB` unnecessarily
- It's already only listed in `src/Makefile.am` (main) and `cli_tests_SOURCES` (tests)

### Alternatives Considered
- **Include in archive, conditionally link boost::program_options**: Complicates the Makefile for no benefit. Rejected.

## Research Task 5: Impact on `SUBDIRS` Ordering and `make check`

### Decision
No change to `SUBDIRS` ordering. `src` is already built before `tests`. The archive is available when test targets link.

### Rationale
- `SUBDIRS = src tests` (existing) ensures `src/libblockchain_core.a` is built before any test binary links against it
- `make check` triggers building `check_PROGRAMS` in `tests/`, which depends on the archive in `src/`
- Automake's generated Makefile correctly handles cross-directory static archive dependencies when referenced in `_LDADD`

## Research Task 6: Cross-Platform Compatibility

### Decision
`noinst_LIBRARIES` with `_a_SOURCES` is standard POSIX Automake — works on Linux (GCC), macOS (Clang), and Windows (MinGW-w64/MSYS2).

### Rationale
- `ar` (archive tool) is available on all three platforms in the CI matrix
- No platform-specific Makefile conditionals needed for the archive itself
- `PLATFORM_LIBS` already handles platform-specific link libraries at the binary level

## Compilation Count Analysis

| State | Core compiles | Target-specific compiles | Total |
|-------|--------------|------------------------|-------|
| Before | 11 × 14 = 154 | 14 (main.cpp + CliParser.cpp + 12 test .cpp + blockchain_tests' 9 .cpp) ≈ 23 | ~177 |
| After | 11 × 1 = 11 | 23 (same target-specific) | ~34 |
| Reduction | **143 fewer compilations** (~81% reduction) | 0 | **~81%** |

Note: `blockchain_tests` has 9 test source files, other test targets have 1 each (12), plus `main.cpp` + `CliParser.cpp` = 23 target-specific.
