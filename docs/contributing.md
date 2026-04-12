# Contributing Guide

## Building

See the [README build instructions](../README.md#build-instructions) for platform-specific setup (Linux, macOS, Windows/MSYS2).

All builds must use parallel compilation:

```bash
make -j8
```

## Running Tests

**Important**: Run each test binary individually rather than using `make check` as a single invocation. This ensures clear per-binary pass/fail reporting.

Build the tests:

```bash
make -j8 -C tests check TESTS=
```

Run each test binary:

```bash
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

All tests use the [Catch2](https://github.com/catchorg/Catch2) framework. New features must include both unit tests and integration tests.

## Coding Conventions

- **C++ standard**: C++20 (`-std=c++20`). Do not use features from later standards.
- **Style**: Follow the conventions already present in the codebase — naming, indentation, brace placement, and header guards via `#pragma once`. There is no formal formatter.
- **Commit messages**: Use imperative mood with a concise summary line (e.g., "Add peer exchange gossip", not "Added" or "Adds").
- **Comments**: Describe *what* or *why*, not planning artifacts or task numbers.

## Dependencies Policy

The approved dependency set is:

| Dependency | Purpose |
|-----------|---------|
| Boost (Asio, Serialization) | Networking and binary persistence |
| OpenSSL | SHA-256 hashing, TLS |
| nlohmann/json | JSON parsing (vendored in `src/json.hpp`) |
| Catch2 | Test framework (test-only) |

Adding a new dependency requires explicit approval and a documented justification.

## Pull Request Workflow

1. Create a feature branch from `main`
2. Implement changes with unit and integration tests
3. Build with `make -j8`
4. Run each test binary individually to confirm all pass
5. Open a pull request for review
6. Merge after review approval and passing CI

Direct commits to `main` are prohibited.
