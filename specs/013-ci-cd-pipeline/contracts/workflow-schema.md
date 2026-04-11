# Contract: GitHub Actions Workflow Schema

**Date**: 2026-04-11

This document defines the contract for the `.github/workflows/ci.yml` file — the single artifact produced by this feature.

## Trigger Contract

The workflow MUST trigger on:

```yaml
on:
  push:
    branches: [main]
  pull_request:
```

- Pushes to `main` only (not all branches).
- All pull requests regardless of target branch.

## Matrix Contract

The workflow MUST define a build matrix with exactly these 4 entries:

| Name | Runner | Compiler | Shell |
|------|--------|----------|-------|
| Linux GCC | `ubuntu-latest` | `gcc-14` / `g++-14` | default (`bash`) |
| Linux Clang | `ubuntu-latest` | `clang-18` / `clang++-18` | default (`bash`) |
| macOS Clang | `macos-latest` | `clang` / `clang++` | default (`bash`) |
| Windows MinGW-w64 | `windows-latest` | `gcc` / `g++` (via MSYS2 UCRT64) | `msys2 {0}` |

`fail-fast` MUST be `false`.

## Build Steps Contract

Each matrix job MUST, in order:

1. **Checkout** source code (`actions/checkout`)
2. **Install dependencies** (platform-specific)
3. **Configure** (`autoreconf -fi && ./configure`)
4. **Build** (`make -j8`)
5. **Test** (each binary as a separate step)

## Test Steps Contract

Each of the following test binaries MUST run as a **separate named step**:

```text
./tests/blockchain_tests
./tests/block_propagation_tests
./tests/block_propagation_integration_tests
./tests/chunk_persistence_tests
./tests/chunk_recovery_tests
./tests/chunk_replace_tests
./tests/merkle_tests
./tests/merkle_rpc_integration_tests
./tests/cli_tests
./tests/rpc_expansion_tests
./tests/lifecycle_tests
./tests/lifecycle_integration_tests
./tests/rpc_integration_tests
./tests/p2p_sync_integration_tests
```

## Status Check Contract

- All 4 matrix jobs MUST complete for the workflow to report overall status.
- The workflow status check MUST be configured as **required** in the repository's branch protection rules for `main`.
- A failing test binary in any job MUST cause that job (and the overall workflow) to fail.

## Timeout Contract

- The overall job timeout SHOULD be set to 60 minutes (to accommodate cold builds).
- Individual test steps SHOULD have a timeout of 5 minutes each.
