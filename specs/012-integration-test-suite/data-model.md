# Data Model: Integration Test Suite

**Date**: 2026-04-11
**Feature**: 012-integration-test-suite

## Entities

### NodeInstance

An in-process blockchain node running on its own `io_context` thread. Each instance is fully isolated — its own data directory, ports, blockchain state, and network servers.

| Field | Type | Description |
|-------|------|-------------|
| data_dir | filesystem path | Unique temporary directory for blockchain data, config, and TLS certs |
| rpc_port | uint16_t | OS-assigned (port 0) RPC listener port |
| p2p_port | uint16_t | OS-assigned (port 0) P2P listener port |
| io_context | boost::asio::io_context | Asio event loop for this node |
| io_thread | std::thread | Background thread running `io_context.run()` |
| blockchain | Blockchain\<Chunk\> | Chain state for this node |
| rpc_server | Server\<RpcServer\> | JSON-RPC TLS acceptor |
| p2p_server | Server\<PeerServer\> | P2P TLS acceptor |
| peer_manager | PeerManager | Peer connection manager |
| block_propagation | BlockPropagation | Block relay logic |
| sync_status | SyncStatus | Sync state tracking |
| cert_path | string | Path to generated self-signed PEM certificate |
| key_path | string | Path to generated PEM private key |

**Lifecycle**: construct → generate_certs → bind_ports → start_servers → `io_thread` running → stop (`io_context.stop()` + thread join) → destroy (RAII cleanup of temp dir)

**Uniqueness**: Each NodeInstance has a unique `data_dir` derived from `temp_directory_path() / unique_suffix`.

---

### TlsCertPair

A self-signed X.509 certificate and RSA-2048 private key written to PEM files in a temporary directory.

| Field | Type | Description |
|-------|------|-------------|
| cert_path | string | Filesystem path to PEM-encoded X.509 certificate |
| key_path | string | Filesystem path to PEM-encoded RSA private key |
| cn | string | Common Name; always "localhost" for test certs |

**Lifecycle**: Generated at test fixture setup; files deleted with temp directory at teardown.

**Validation**: Certificate is V3, SHA-256-signed, 365-day validity, serial=1.

---

### RpcTestClient

A synchronous TLS client for issuing JSON-RPC requests to a NodeInstance's RPC port and parsing responses.

| Field | Type | Description |
|-------|------|-------------|
| host | string | Always "127.0.0.1" or "::1" for loopback connections |
| port | uint16_t | Target RPC port (obtained from NodeInstance after start) |
| ssl_context | ssl::context | TLS context with verification disabled (self-signed) |
| socket | ssl::stream\<tcp::socket\> | Connected TLS socket |

**Operations**:
- `connect()` — resolve, TCP connect, TLS handshake
- `send(json)` — serialize JSON + newline, write to socket
- `receive()` → json — read until newline, parse JSON
- `call(method, params)` → json — send request + receive response (convenience)

**Validation**: Must check `jsonrpc` field = "2.0" and presence of either `result` or `error` in response.

---

### IntegrationTestFixture

Manages the lifecycle of one or more NodeInstances within a Catch2 test case.

| Field | Type | Description |
|-------|------|-------------|
| nodes | vector\<NodeInstance\> | Active node instances for this test |
| base_temp_dir | filesystem path | Root temporary directory containing all node subdirs |

**Lifecycle**: Constructor creates `base_temp_dir`; each `create_node()` call adds a NodeInstance. Destructor stops all nodes and removes `base_temp_dir`.

**Invariants**:
- All nodes are stopped before temp directory cleanup.
- Cleanup runs even on test failure (RAII destructor guarantee).
- Each node gets a unique subdirectory under `base_temp_dir`.

## Relationships

```
IntegrationTestFixture 1──* NodeInstance
NodeInstance 1──1 TlsCertPair
NodeInstance 1──1 Blockchain<Chunk>
NodeInstance 1──1 Server<RpcServer>
NodeInstance 1──1 Server<PeerServer>
NodeInstance 1──1 PeerManager
NodeInstance 1──1 BlockPropagation
RpcTestClient *──1 NodeInstance (connects to its rpc_port)
```

## State Transitions

### NodeInstance Lifecycle

```
[Created] → generate_certs() → [Configured]
[Configured] → bind_ports() + start_servers() + start_thread() → [Running]
[Running] → io_context.stop() + thread.join() → [Stopped]
[Stopped] → cleanup temp_dir → [Destroyed]
```

### Test Flow

```
setup: create fixture → create node(s) → start node(s)
test: create RpcTestClient → connect → call methods → assert responses
      OR: add blocks to node A → wait for sync on node B → assert
teardown: stop all nodes → remove temp dirs
```
