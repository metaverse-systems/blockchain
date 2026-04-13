# Quickstart: 018-audit-bug-security-fixes

**Date**: 2026-04-13

## Build

```bash
cd /home/tim/projects/metaverse-systems/blockchain
make -j8
```

## Run Tests

```bash
./tests/blockchain_tests
./tests/sync_tests
./tests/utils_tests
./tests/rpc_integration_tests
```

## Verify Individual Fixes

### §2.1 — Sync appends blocks

Run the sync tests and verify the "handle_sync_response appends new blocks"
test passes:

```bash
./tests/sync_tests -c "handle_sync_response appends"
```

### §2.4 — parsePeerKey port validation

```bash
./tests/utils_tests -c "parsePeerKey"
```

### §3.1 — Seed node port parsing

Start the node with an invalid seed node and verify it exits gracefully:

```bash
./src/blockchain --data-dir /tmp/test-node --seed-node "host:abc"
# Expected: error message, non-zero exit code

./src/blockchain --data-dir /tmp/test-node --seed-node "host:99999"
# Expected: error message, non-zero exit code
```

### §3.2 — RPC getBlockByIndex bounds check

Start a node and send an out-of-range RPC request:

```bash
# In one terminal:
./src/blockchain --data-dir /tmp/test-node

# In another terminal:
echo '{"jsonrpc":"2.0","id":1,"method":"getBlockByIndex","params":{"index":999999}}' | \
  openssl s_client -connect localhost:8332 -quiet 2>/dev/null
# Expected: JSON-RPC error with code -32001
```
