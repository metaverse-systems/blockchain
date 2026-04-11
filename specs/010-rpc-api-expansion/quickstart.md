# Quickstart: RPC API Expansion

**Feature**: 010-rpc-api-expansion  
**Prerequisites**: Running blockchain node with TLS certificates configured

## Verify New Endpoints

All commands use `openssl s_client` to connect to the TLS-wrapped JSON-RPC server. Adjust host/port as needed.

### 1. Check Node Status

```bash
echo '{"jsonrpc":"2.0","method":"getNodeStatus","params":{},"id":"1"}' | \
  openssl s_client -connect localhost:12346 -quiet 2>/dev/null
```

Expected response includes `chainLength`, `chunkCount`, `syncState`, `currentDifficulty`, `inboundPeers`, `outboundPeers`, and `nodeUuid`.

### 2. Get Chain Length

```bash
echo '{"jsonrpc":"2.0","method":"getChainLength","params":{},"id":"2"}' | \
  openssl s_client -connect localhost:12346 -quiet 2>/dev/null
```

Expected: `{"jsonrpc":"2.0","result":<integer>,"id":"2"}`

### 3. Get Chunk Count

```bash
echo '{"jsonrpc":"2.0","method":"getChunkCount","params":{},"id":"3"}' | \
  openssl s_client -connect localhost:12346 -quiet 2>/dev/null
```

Expected: `{"jsonrpc":"2.0","result":<integer>,"id":"3"}`

### 4. Fetch a Block Range (full blocks)

```bash
echo '{"jsonrpc":"2.0","method":"getBlockRange","params":{"startIndex":0,"endIndex":5},"id":"4"}' | \
  openssl s_client -connect localhost:12346 -quiet 2>/dev/null
```

Expected: JSON array of full block objects (with `entries` field).

### 5. Fetch a Block Range (headers only)

```bash
echo '{"jsonrpc":"2.0","method":"getBlockRange","params":{"startIndex":0,"endIndex":5,"headersOnly":true},"id":"5"}' | \
  openssl s_client -connect localhost:12346 -quiet 2>/dev/null
```

Expected: JSON array of header-only block objects (no `entries` field).

### 6. Verify Error Handling — Invalid Range

```bash
echo '{"jsonrpc":"2.0","method":"getBlockRange","params":{"startIndex":10,"endIndex":5},"id":"6"}' | \
  openssl s_client -connect localhost:12346 -quiet 2>/dev/null
```

Expected: Error response with code `-32602` and message about invalid range.

## Run Tests

```bash
make check
```

All new test cases in `rpc_expansion_tests` should pass.
