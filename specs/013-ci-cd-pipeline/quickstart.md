# Quickstart: CI/CD Pipeline

**Date**: 2026-04-11

## What This Feature Does

Adds a GitHub Actions CI workflow (`.github/workflows/ci.yml`) that automatically builds and tests the blockchain project on Linux, macOS, and Windows whenever code is pushed to `main` or a pull request is opened.

## How to Verify

### 1. Check the workflow file exists

```bash
cat .github/workflows/ci.yml
```

### 2. Push to a branch and open a PR

```bash
git checkout -b test-ci
git commit --allow-empty -m "Test CI pipeline"
git push origin test-ci
# Open a pull request on GitHub
```

### 3. Verify CI runs

- Navigate to the pull request on GitHub.
- Look for the "CI" status check in the Checks tab.
- Verify 4 matrix jobs are running: Linux GCC, Linux Clang, macOS Clang, Windows MinGW-w64.
- Each job should show individual test binary steps.

### 4. Verify merge blocking

- While CI is running or failing, the "Merge" button should be disabled.
- After all 4 jobs pass, the "Merge" button should be enabled.

### 5. Verify caching

- Push a second commit to the same PR.
- Check the Windows job — it should restore MSYS2 packages from cache.
- The second run should complete faster than the first.

## Matrix Configuration

| Job Name | OS | Compiler | Notes |
|----------|-----|----------|-------|
| Linux GCC | Ubuntu 24.04 | GCC 14 | Default compiler |
| Linux Clang | Ubuntu 24.04 | Clang 18 | Tests portability |
| macOS Clang | macOS (ARM) | Apple Clang | Homebrew OpenSSL needed |
| Windows MinGW-w64 | Windows Server 2022 | MinGW-w64 GCC (UCRT64) | MSYS2 shell |

## Build Commands Used in CI

```bash
# Configure
autoreconf -fi
./configure

# Build (constitution mandates -j8)
make -j8

# Test (each binary individually, per constitution III)
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
