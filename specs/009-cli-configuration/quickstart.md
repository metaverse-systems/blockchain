# Quickstart: CLI & Configuration

**Feature**: 009-cli-configuration
**Date**: 2026-04-11

## Prerequisites

- Built blockchain binary (`make` from repo root)
- A blockchain data directory (e.g., `./bc-dir/`)

## Quick Validation Steps

### 1. Help output

```bash
./src/blockchain --help
```

**Expected**: Structured help listing all options with defaults. Exit code 0.

### 2. Version output

```bash
./src/blockchain --version
```

**Expected**: `blockchain 0.0.1` (or current version). Exit code 0.

### 3. Generate default config

```bash
mkdir -p /tmp/test-chain
./src/blockchain --generate-config /tmp/test-chain
```

**Expected**: `/tmp/test-chain/config.json` and `/tmp/test-chain/config.README` created. Exit code 0.

### 4. Refuse to overwrite existing config

```bash
./src/blockchain --generate-config /tmp/test-chain
```

**Expected**: Error message about existing config.json. Exit code 1.

### 5. Override ports via CLI

```bash
./src/blockchain --rpc-port 9999 --p2p-port 9998 ./bc-dir/
```

**Expected**: Daemon starts, RPC on port 9999, P2P on port 9998.

### 6. Set log level

```bash
./src/blockchain --log-level debug ./bc-dir/
```

**Expected**: Debug-level messages visible in output.

### 7. Validation catches errors

Create a bad config:
```bash
echo '{"network": {"rpc_port": 99999}}' > /tmp/test-chain/config.json
./src/blockchain /tmp/test-chain
```

**Expected**: Error message about invalid port range. Exit code 1.

### 8. Unknown keys warning

```bash
echo '{"network": {"rpc_port": 12345}, "unknown_section": {}}' > /tmp/test-chain/config.json
./src/blockchain /tmp/test-chain
```

**Expected**: Warning about unrecognized key `unknown_section`, then normal startup (or fail on TLS — that's expected without certs).

### 9. Alternative config path

```bash
./src/blockchain --config /tmp/test-chain/config.json ./bc-dir/
```

**Expected**: Config loaded from the specified path instead of `./bc-dir/config.json`.

### 10. Run tests

```bash
make check
```

**Expected**: All tests pass, including new `cli_tests`.

## Cleanup

```bash
rm -rf /tmp/test-chain
```
