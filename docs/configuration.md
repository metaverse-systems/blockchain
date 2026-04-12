# Configuration Reference

This document covers all CLI flags, `config.json` fields, and TLS certificate setup for the blockchain node.

## Table of Contents

- [CLI Flags](#cli-flags)
- [CLI Precedence Rules](#cli-precedence-rules)
- [config.json Schema](#configjson-schema)
  - [TLS Section](#tls-section)
  - [Network Section](#network-section)
  - [Consensus Section](#consensus-section)
  - [Peers Section](#peers-section)
  - [Streams Section](#streams-section)
  - [Persistence Section](#persistence-section)
- [TLS Setup](#tls-setup)
- [Complete Default config.json](#complete-default-configjson)

## CLI Flags

| Flag | Short | Type | Default | Description |
|------|-------|------|---------|-------------|
| `--help` | `-h` | bool | — | Show help message and exit |
| `--version` | `-v` | bool | — | Show version information and exit |
| `--config` | — | string | `<blockchain-dir>/config.json` | Path to configuration file |
| `--rpc-port` | — | uint16 | 12345 | JSON-RPC listen port |
| `--p2p-port` | — | uint16 | 12346 | P2P listen port |
| `--seed-node` | — | string[] | `[]` | Add seed node (repeatable, format: `host:port`) |
| `--log-level` | — | string | `info` | Log verbosity: `debug`, `info`, `warning`, `error` |
| `--generate-config` | — | bool | — | Generate default `config.json` in the blockchain directory and exit |
| *(positional)* | — | string | *(required)* | Path to blockchain data directory |

**Examples:**

```bash
# Start with defaults
./blockchain /path/to/data

# Custom ports and log level
./blockchain --rpc-port 8080 --p2p-port 8081 --log-level debug /path/to/data

# Generate a default config file
./blockchain --generate-config /path/to/data

# Add seed nodes
./blockchain --seed-node 10.0.0.1:12346 --seed-node 10.0.0.2:12346 /path/to/data
```

## CLI Precedence Rules

Configuration values are resolved in this order (highest priority first):

1. **CLI flags** — always override everything
2. **config.json** — loaded from the blockchain directory (or `--config` path)
3. **Built-in defaults** — used when neither CLI nor config.json specifies a value

**Exit codes:**

| Code | Meaning |
|------|---------|
| 0 | Success / normal exit |
| 1 | Invalid arguments or configuration error |
| 2 | Runtime error (e.g., port already in use, missing TLS files) |

## config.json Schema

The configuration file is a JSON object organized into six sections. All fields are optional — omitted fields use their built-in defaults.

### TLS Section

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `tls.cert_file` | string | `"cert.pem"` | TLS certificate file path (relative to blockchain directory) |
| `tls.key_file` | string | `"key.pem"` | TLS private key file path (relative to blockchain directory) |
| `tls.ca_file` | string | `""` | CA certificate for peer verification. Empty string disables CA verification |

Both `cert_file` and `key_file` must exist and be non-empty. Both the RPC server and P2P connections use TLS.

### Network Section

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `network.rpc_port` | uint16 | `12345` | JSON-RPC listen port |
| `network.p2p_port` | uint16 | `12346` | P2P listen port |
| `network.timeout_seconds` | uint32 | `30` | Connection timeout in seconds |
| `network.log_level` | string | `"info"` | Log verbosity: `debug`, `info`, `warning`, `error` |

Validation: `rpc_port` and `p2p_port` must be in the range 1–65535 and must not be equal.

### Consensus Section

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `consensus.target_block_interval` | uint32 | `10` | Target seconds between blocks |
| `consensus.adjustment_window` | uint32 | `10` | Number of blocks between difficulty adjustments |
| `consensus.max_adjustment_factor` | double | `4.0` | Maximum difficulty change ratio per adjustment |
| `consensus.min_difficulty` | uint32 | `1` | Minimum PoW difficulty (leading zero bits) |
| `consensus.max_difficulty` | uint32 | `16` | Maximum PoW difficulty (leading zero bits) |
| `consensus.initial_difficulty` | uint32 | `1` | Starting difficulty for new chains |
| `consensus.mining_timeout` | uint32 | `30` | Maximum seconds to spend mining one block |
| `consensus.max_future_timestamp` | uint32 | `120` | Maximum seconds a block timestamp may be ahead of local time |
| `consensus.max_reorg_depth` | uint32 | `100` | Maximum chain reorganization depth |

The difficulty adjustment algorithm uses a log₂ ratio of actual vs. target interval, clamped to `max_adjustment_factor`, to keep block times near `target_block_interval`.

### Peers Section

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `peers.seed_nodes` | array | `[]` | Initial peers as `[{"host": "...", "port": ...}]` objects |
| `peers.max_outbound` | uint32 | `8` | Maximum outbound peer connections |
| `peers.max_inbound` | uint32 | `32` | Maximum inbound peer connections |
| `peers.exchange_interval_seconds` | uint32 | `30` | Peer exchange gossip interval in seconds |
| `peers.discovery_enabled` | bool | `true` | Enable automatic peer discovery via gossip |
| `peers.max_stored_peers` | uint32 | `256` | Maximum peers to persist in `peers.json` |
| `peers.reconnect_base_delay_seconds` | uint32 | `5` | Initial reconnection delay in seconds |
| `peers.reconnect_max_delay_seconds` | uint32 | `300` | Maximum reconnection backoff in seconds |
| `peers.ban_threshold_errors` | uint32 | `10` | Protocol errors before automatic ban |
| `peers.ban_duration_seconds` | uint32 | `3600` | Default ban duration in seconds |

Validation constraints:
- `max_outbound` and `max_inbound` must be > 0
- `exchange_interval_seconds` must be ≥ 5
- `reconnect_base_delay_seconds` must be ≥ 1
- `reconnect_max_delay_seconds` must be ≥ `reconnect_base_delay_seconds`
- `ban_threshold_errors` must be ≥ 1

### Streams Section

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `streams.allowed_streams` | array | `[]` | Stream names this node may publish to. Empty array means all streams are allowed |

### Persistence Section

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `persistence.save_interval_seconds` | uint32 | `300` | Periodic chunk save interval in seconds |
| `persistence.fast_startup` | bool | `false` | Skip full chain validation on recovery (faster startup, less safety) |

## TLS Setup

Both the JSON-RPC server and P2P connections require TLS. You need a CA certificate, a server certificate, and a private key for each node.

### Step 1: Generate a Certificate Authority

```bash
# Generate CA private key
openssl genpkey -algorithm RSA -out ca-key.pem -pkeyopt rsa_keygen_bits:2048

# Generate self-signed CA certificate
openssl req -new -x509 -key ca-key.pem -out ca.pem -days 365 \
  -subj "/CN=Blockchain CA"
```

### Step 2: Generate a Server Certificate

```bash
# Generate server private key
openssl genpkey -algorithm RSA -out key.pem -pkeyopt rsa_keygen_bits:2048

# Generate certificate signing request
openssl req -new -key key.pem -out server.csr -subj "/CN=localhost"

# Sign with the CA
openssl x509 -req -in server.csr -CA ca.pem -CAkey ca-key.pem -CAcreateserial \
  -out cert.pem -days 365
```

### Step 3: Place Files in the Blockchain Directory

Copy `cert.pem`, `key.pem`, and `ca.pem` into the blockchain data directory:

```bash
cp cert.pem key.pem ca.pem /path/to/blockchain-dir/
```

The `config.json` fields `tls.cert_file`, `tls.key_file`, and `tls.ca_file` reference these filenames. Paths are relative to the blockchain directory.

### Multi-Node Setup

For a multi-node network, each node needs its own key and certificate, but they can share the same CA:

1. Generate one CA (step 1 above)
2. Generate a separate key + certificate for each node (repeat step 2)
3. Distribute the shared `ca.pem` to all nodes

Setting `tls.ca_file` to the CA certificate enables peer certificate verification on P2P connections.

## Complete Default config.json

```json
{
  "tls": {
    "cert_file": "cert.pem",
    "key_file": "key.pem",
    "ca_file": ""
  },
  "network": {
    "rpc_port": 12345,
    "p2p_port": 12346,
    "timeout_seconds": 30,
    "log_level": "info"
  },
  "consensus": {
    "target_block_interval": 10,
    "adjustment_window": 10,
    "max_adjustment_factor": 4.0,
    "min_difficulty": 1,
    "max_difficulty": 16,
    "initial_difficulty": 1,
    "mining_timeout": 30,
    "max_future_timestamp": 120,
    "max_reorg_depth": 100
  },
  "peers": {
    "seed_nodes": [],
    "max_outbound": 8,
    "max_inbound": 32,
    "exchange_interval_seconds": 30,
    "discovery_enabled": true,
    "max_stored_peers": 256,
    "reconnect_base_delay_seconds": 5,
    "reconnect_max_delay_seconds": 300,
    "ban_threshold_errors": 10,
    "ban_duration_seconds": 3600
  },
  "streams": {
    "allowed_streams": []
  },
  "persistence": {
    "save_interval_seconds": 300,
    "fast_startup": false
  }
}
```
