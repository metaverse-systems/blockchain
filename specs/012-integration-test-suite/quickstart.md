# Quickstart: Integration Test Suite

**Feature**: 012-integration-test-suite
**Date**: 2026-04-11

## Prerequisites

- C++20 compiler (GCC 10+, Clang 11+, MSVC 19.29+)
- Boost (Asio, Serialization)
- OpenSSL 1.1.0+ (development headers + libraries)
- Catch2 (development headers + libraries)
- GNU Autotools (autoconf, automake, libtool)

## Build

```bash
# Build everything including integration tests
make -j8
```

## Run Integration Tests

Per constitution, run each test binary individually:

```bash
# RPC endpoint integration tests (single node over real TLS)
./tests/rpc_integration_tests

# P2P sync integration tests (two-node sync over real TLS)
./tests/p2p_sync_integration_tests
```

## What the Tests Do

### rpc_integration_tests

1. Generates a self-signed TLS certificate and RSA key at test startup
2. Starts a blockchain node in-process on a background thread with dynamic ports
3. Connects an RPC test client over TLS
4. Exercises every JSON-RPC method with valid requests and verifies responses
5. Sends malformed/invalid requests and verifies error responses
6. Stops the node and cleans up all temporary files

### p2p_sync_integration_tests

1. Generates TLS certificates for two nodes
2. Starts Node A with 10 pre-published blocks
3. Starts Node B with only genesis, configured to connect to Node A as a peer
4. Verifies Node B synchronizes its chain to match Node A
5. Publishes a new block on Node A, verifies it propagates to Node B
6. Stops all nodes and cleans up

## Validation Checklist

- [ ] `make -j8` compiles both new test binaries without errors
- [ ] `./tests/rpc_integration_tests` passes all test cases (exit code 0)
- [ ] `./tests/p2p_sync_integration_tests` passes all test cases (exit code 0)
- [ ] No temporary files or directories left behind after test completion
- [ ] No zombie threads after test completion
- [ ] Tests complete within 60 seconds each on standard hardware
