# Research: CI/CD Pipeline

**Date**: 2026-04-11

## R1: Windows Build Toolchain (MSYS2)

**Decision**: Use `msys2/setup-msys2@v2` action with UCRT64 msystem and MinGW-w64 GCC.

**Rationale**: MSYS2 is pre-installed on `windows-latest` runners. The `msys2/setup-msys2@v2` action configures it properly, provides a `msys2 {0}` shell, and handles package caching automatically. UCRT64 is the recommended msystem over MINGW64. Autotools runs natively under the MSYS layer while the compiler toolchain is MinGW.

**Alternatives considered**:
- MSVC + CMake: Rejected — would require a second build system, violating the constitution.
- Cygwin: Rejected — heavier, slower, and MSYS2 is already pre-installed on runners.

**Key packages**: `mingw-w64-ucrt-x86_64-gcc`, `mingw-w64-ucrt-x86_64-boost`, `mingw-w64-ucrt-x86_64-openssl`, `mingw-w64-ucrt-x86_64-catch2`, `autotools`, `make`, `pkg-config`.

**Gotchas**: Line ending conversion (use `core.autocrlf input`). Default shell must be `msys2 {0}`.

## R2: Linux Dependency Strategy

**Decision**: Use apt packages on `ubuntu-latest` (Ubuntu 24.04).

**Rationale**: All required dependencies are available via apt. GCC 14 and Clang 18 are pre-installed. Autotools, make, pkg-config, and libssl-dev are pre-installed.

**Alternatives considered**:
- Building dependencies from source: Rejected — unnecessary, apt packages are sufficient and faster.
- Using Ubuntu 22.04: Rejected — `catch2` apt package on 22.04 is Catch2 v2 only; 24.04 provides Catch2 v3.4.0 with `catch2-with-main` pkg-config support.

**Required apt packages**: `libboost-serialization-dev`, `libboost-program-options-dev`, `libssl-dev`, `catch2`. Pre-installed tools (autoconf, automake, make, g++, pkg-config) do not need explicit installation.

**Compiler switching**: Set `CC=gcc-14 CXX=g++-14` for GCC; `CC=clang-18 CXX=clang++-18` for Clang. No extra packages needed.

## R3: macOS Dependency Strategy

**Decision**: Use Homebrew on `macos-latest` (Apple Silicon).

**Rationale**: Homebrew and Xcode CLI tools are pre-installed. All required packages (boost, openssl@3, catch2, autoconf, automake, libtool, pkg-config) are available via Homebrew.

**Alternatives considered**:
- MacPorts: Rejected — not pre-installed on GitHub Actions runners.
- Building from source: Rejected — unnecessary overhead.

**Key packages**: `boost`, `openssl@3`, `catch2`, `autoconf`, `automake`, `libtool`, `pkg-config`.

**Gotchas**: macOS ships LibreSSL, not OpenSSL. Homebrew's OpenSSL is keg-only. Must export `PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig"` so `configure` finds it.

## R4: Caching Strategy

**Decision**: Rely on MSYS2 built-in caching for Windows. Do not cache apt/Homebrew packages. Optionally cache build artifacts keyed on configure.ac/Makefile.am hashes.

**Rationale**: `msys2/setup-msys2@v2` has built-in caching (`cache: true` by default). Apt and Homebrew package installation is fast (~10-30s) and cache invalidation is fragile. The main CI time cost is compilation, not package installation.

**Alternatives considered**:
- Caching apt packages via `actions/cache`: Rejected — fragile invalidation, minimal time savings.
- Caching Homebrew downloads: Rejected — same reasoning.
- Caching compiled object files: Considered but risky with autotools (stale .o files). Deferred — if builds are slow, can add `ccache` later.

**Cache limits**: 10 GB per repository total, 7-day eviction for unused caches, branch-scoped.

## R5: Matrix Strategy

**Decision**: Use `include`-based matrix with `fail-fast: false` for 4 configurations.

**Rationale**: `include`-based matrix gives explicit control over each configuration's OS, compiler, and name. `fail-fast: false` ensures all platforms report results even if one fails.

**Matrix entries**:
| Name | OS | CC | CXX |
|------|----|----|-----|
| Linux GCC | ubuntu-latest | gcc-14 | g++-14 |
| Linux Clang | ubuntu-latest | clang-18 | clang++-18 |
| macOS Clang | macos-latest | clang | clang++ |
| Windows MinGW-w64 | windows-latest | gcc | g++ |

**Alternatives considered**:
- `fail-fast: true`: Rejected — hides failures on other platforms.
- `continue-on-error` for Windows: Considered but rejected — Windows is a required platform per constitution VII.

## R6: Test Execution Strategy

**Decision**: Each of 14 test binaries runs as a separate CI step using `run: ./tests/<binary>`.

**Rationale**: Constitution III requires individual test binary execution. Separate steps provide per-binary pass/fail visibility in the GitHub Actions UI without parsing logs.

**Test binaries** (from `tests/Makefile.am` `check_PROGRAMS`):
1. `blockchain_tests`
2. `block_propagation_tests`
3. `block_propagation_integration_tests`
4. `chunk_persistence_tests`
5. `chunk_recovery_tests`
6. `chunk_replace_tests`
7. `merkle_tests`
8. `merkle_rpc_integration_tests`
9. `cli_tests`
10. `rpc_expansion_tests`
11. `lifecycle_tests`
12. `lifecycle_integration_tests`
13. `rpc_integration_tests`
14. `p2p_sync_integration_tests`

**Alternatives considered**:
- `make check`: Rejected — constitution explicitly prohibits monolithic test invocation.
- Single step with a for-loop: Rejected — clarification session decided separate steps.
