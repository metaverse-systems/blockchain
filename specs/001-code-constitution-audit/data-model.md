# Data Model: Code Constitution Audit & Remediation

**Feature Branch**: `001-code-constitution-audit`
**Date**: 2026-04-10

---

## Entities

This feature modifies existing entities rather than creating new ones. The data model below documents the **changes** to each entity.

### Block (unchanged)

| Field | Type | Description |
|-------|------|-------------|
| index | `size_t` | Sequential block index |
| timestamp | `uint64_t` | Creation time (epoch) |
| data | `std::string` | Block payload |
| prevHash | `std::string` | SHA-256 hash of previous block |
| hash | `std::string` | SHA-256 hash of this block |

**Relationships**: Contained in a `Chunk`. Linked to predecessor via `prevHash`.
**Validation**: `calculateHash()` must match `hash`; `prevHash` must match predecessor's `hash`.
**State transitions**: Immutable once added to chain.
**Changes**: None.

---

### Chunk (modified)

| Field | Type | Description |
|-------|------|-------------|
| blocks | `std::vector<Block>` | Up to 100 blocks |
| index | `std::size_t` | Chunk sequence number |
| blockchainPath | `std::filesystem::path` | Base directory for persistence |

**Relationships**: Owned by `Blockchain::chain` vector.
**Validation**: `blocks.size() <= 100`; `isBlockPresent()` depends on sequential index invariant.

**Changes**:
- **FR-006**: `Chunk::save()` — replace `throw new std::runtime_error(...)` with `throw std::runtime_error(...)`
- **FR-007**: `Chunk::save()` and `Chunk::load()` — replace `blockchainPath.string() + ss.str()` with `blockchainPath / filename`

---

### Blockchain\<ChunkHandler\> (modified)

| Field | Type | Description |
|-------|------|-------------|
| chain | `std::vector<ChunkHandler>` | Ordered list of chunks |
| keyIndexMap | `std::map<std::string, std::vector<size_t>>` | Key → block index mapping |
| blockchainPath | `std::filesystem::path` | Base directory for persistence |
| strand | `boost::asio::strand<boost::asio::io_context::executor_type>` | **NEW** — serializes mutation access |

**Relationships**: Owns `ChunkHandler` instances. Referenced by `Server<>` instances.
**Validation**: Genesis block at index 0; `isValidNewBlock()` for each addition.

**Changes**:
- **FR-010**: `addBlock()` — change `auto currentChunk = this->chain.back()` to `auto& currentChunk = this->chain.back()`
- **FR-012**: `getBlocksByKeys()` — group indices by chunk; load each chunk at most once
- **FR-013**: Add strand member; wrap `addBlock`, `saveChunk`, `loadChunk`, `saveKeys`, `loadKeys` in strand dispatch
- **FR-007**: `saveKeys()` and `loadKeys()` — replace string concat paths with `operator/`

---

### SessionHandler (modified)

| Field | Type | Description |
|-------|------|-------------|
| ssl_socket | `ssl::stream<tcp::socket>` | TLS-wrapped socket |
| bc | `IBlockchain&` | Reference to blockchain |
| timeout_timer | `boost::asio::steady_timer` | **NEW** — async op timeout |
| timeout_duration | `std::chrono::seconds` | **NEW** — configurable (default 30s) |

**Relationships**: Base class for `RpcServer` and `PeerServer`.

**Changes**:
- **FR-011**: Move SSL async handshake from `RpcServer::start()` and `PeerServer::start()` into `SessionHandler::start()`
- **FR-008**: Add structured error logging in handshake failure handler
- **FR-009**: Add `steady_timer` alongside async operations; cancel on completion or close on timeout

---

### RpcServer (modified)

**Changes**:
- **FR-004b**: Operates under server-only TLS context (no client cert required)
- **FR-011**: Delegates SSL handshake to `SessionHandler::start()`; overrides `on_handshake_complete()` for RPC-specific read loop

---

### PeerServer (modified)

**Changes**:
- **FR-004a**: Operates under mutual TLS context (both sides present certs)
- **FR-011**: Delegates SSL handshake to `SessionHandler::start()`; overrides `on_handshake_complete()` for P2P-specific header read

---

### New: Utility Functions (in utils.hpp / utils.cpp)

| Function | Signature | Description |
|----------|-----------|-------------|
| `loadDotEnv` | `void loadDotEnv(const std::filesystem::path& path)` | Parses `.env` file, injects KEY=VALUE into process environment |
| `logMessage` | `void logMessage(const std::string& level, const std::string& msg)` | Writes `[TIMESTAMP] [LEVEL] msg` to stderr |

---

## Configuration

### Environment Variables

| Variable | Required | Description |
|----------|----------|-------------|
| `BLOCKCHAIN_CERT_FILE` | Yes | Path to TLS certificate file (PEM) |
| `BLOCKCHAIN_KEY_FILE` | Yes | Path to TLS private key file (PEM) |
| `BLOCKCHAIN_CA_FILE` | For P2P | Path to CA certificate for peer verification |
| `BLOCKCHAIN_TIMEOUT` | No | Async operation timeout in seconds (default: 30) |

### .env File Location

The `.env` file is read from the **blockchain data directory** (the path supplied as the first command-line argument to the daemon), not the current working directory. For example, if the daemon is started with `./blockchain /data/chain`, the loader reads `/data/chain/.env`.

### .env File Format

```
# TLS configuration  
BLOCKCHAIN_CERT_FILE=/path/to/cert.pem
BLOCKCHAIN_KEY_FILE=/path/to/key.pem
BLOCKCHAIN_CA_FILE=/path/to/ca.pem

# Optional
BLOCKCHAIN_TIMEOUT=30
```
