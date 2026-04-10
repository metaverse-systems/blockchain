# Quickstart: Chain Synchronization

**Feature**: 003-chain-sync
**Date**: 2026-04-10

## Prerequisites

- Two blockchain nodes built from `003-chain-sync` branch
- TLS certificates configured (`.env` with `BLOCKCHAIN_CERT_FILE`, `BLOCKCHAIN_KEY_FILE`, `BLOCKCHAIN_CA_FILE`)
- Two separate blockchain data directories (e.g., `./node-a/` and `./node-b/`)

## Build

```bash
./autogen.sh
./configure
make
make check
```

## Scenario 1: Initial Sync (New Node Joins)

### 1. Start Node A with some blocks

```bash
# Start node A
./src/blockchain ./node-a/

# In another terminal, add some blocks via RPC
echo '{"jsonrpc":"2.0","id":"1","method":"addBlock","params":{"data":"block 1","keys":["test"]}}' | \
  openssl s_client -connect localhost:12345 -quiet

echo '{"jsonrpc":"2.0","id":"2","method":"addBlock","params":{"data":"block 2","keys":["test"]}}' | \
  openssl s_client -connect localhost:12345 -quiet
```

### 2. Start Node B (fresh)

```bash
# Start node B on different ports (requires port configuration)
./src/blockchain ./node-b/
```

### 3. Connect Node B to Node A

Node B connects to Node A's P2P port (12346 by default). Upon TLS handshake completion, Node B automatically sends a `BLOCKCHAIN_QUERY` with its local chain height (1, genesis only). Node A responds with the missing blocks chunk by chunk.

### 4. Verify sync

```bash
# Query Node B for a block that was on Node A
echo '{"jsonrpc":"2.0","id":"1","method":"getBlockByIndex","params":{"index":1}}' | \
  openssl s_client -connect localhost:12345 -quiet
```

Node B should return the same block data as Node A.

## Scenario 2: Manual Sync via RPC

```bash
# Trigger sync manually
echo '{"jsonrpc":"2.0","id":"1","method":"requestSync","params":{}}' | \
  openssl s_client -connect localhost:12345 -quiet
```

**Expected response**:
```json
{"jsonrpc":"2.0","id":"1","result":"sync_started"}
```

## Scenario 3: addBlock Blocked During Sync

While a sync is in progress:

```bash
echo '{"jsonrpc":"2.0","id":"1","method":"addBlock","params":{"data":"blocked","keys":["test"]}}' | \
  openssl s_client -connect localhost:12345 -quiet
```

**Expected response**:
```json
{"jsonrpc":"2.0","id":"1","error":{"code":-32001,"message":"Node is syncing","data":"addBlock is unavailable while chain synchronization is in progress"}}
```

## What to Test

1. **Initial sync**: Node B catches up to Node A's chain length
2. **Incremental sync**: Stop Node B, add blocks to Node A, restart Node B — it downloads only the missing blocks
3. **Validation**: Modify a block's hash in transit (test fixture) — Node B rejects the invalid chunk
4. **Timeout**: Disconnect mid-sync — Node B preserves its existing chain
5. **RPC during sync**: Read-only queries work; addBlock returns error
