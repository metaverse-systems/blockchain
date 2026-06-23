# Quickstart: Peer Disconnect Test Coverage

## Build

```bash
make -j8
```

## Run Tests

```bash
# Run block propagation tests (includes new relay exception tests)
./tests/block_propagation_tests

# Run peer manager tests (includes new disconnect handler tests)
./tests/peer_manager_tests
```

## Run Specific Test Cases

```bash
# Relay exception tests
./tests/block_propagation_tests "[relay_exception]"

# Disconnect handler tests
./tests/peer_manager_tests "[disconnect]"
```

## Verify All Tests Pass

```bash
./tests/block_propagation_tests
./tests/peer_manager_tests
./tests/lifecycle_tests
./tests/sync_tests
./tests/consensus_tests
```
