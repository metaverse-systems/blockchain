# Data Model: CLI & Configuration

**Feature**: 009-cli-configuration
**Date**: 2026-04-11

## Entities

### LogLevel (enum)

Represents the verbosity threshold for log output.

| Value | Numeric | Description |
|-------|---------|-------------|
| `Debug` | 0 | Verbose diagnostic output |
| `Info` | 1 | Normal operational messages (default) |
| `Warning` | 2 | Recoverable issues or degraded conditions |
| `Error` | 3 | Failures requiring attention |

**Relationships**: Used by `logMessage()` for filtering. Set at startup from CLI or config.json. Stored as `std::atomic<LogLevel>` global.

**Validation**: Must be one of the four defined values. CLI input is case-insensitive. Config.json value is a lowercase string.

---

### CliOptions (struct)

Parsed command-line arguments before merging with config file values.

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `blockchain_dir` | `std::string` | (required) | Positional argument: path to blockchain data directory |
| `config_path` | `std::optional<std::string>` | empty | `--config <path>`: alternative config.json location |
| `rpc_port` | `std::optional<uint16_t>` | empty | `--rpc-port <port>`: override RPC port |
| `p2p_port` | `std::optional<uint16_t>` | empty | `--p2p-port <port>`: override P2P port |
| `seed_nodes` | `std::vector<std::string>` | empty | `--seed-node <host:port>`: additional seed nodes |
| `log_level` | `std::optional<std::string>` | empty | `--log-level <level>`: override log level |
| `generate_config` | `bool` | false | `--generate-config`: write default config and exit |
| `show_help` | `bool` | false | `--help` / `-h`: show usage and exit |
| `show_version` | `bool` | false | `--version` / `-v`: show version and exit |

**Relationships**: Parsed from `argc`/`argv` by `CliParser::parse()`. Merged into `NodeConfig` in `main.cpp` after config file is loaded.

**Validation**: Boost.ProgramOptions validates types (e.g., port is numeric). Additional validation (port range, path existence) occurs after merging into `NodeConfig`.

---

### NodeConfig::NetworkConfig (extended)

Existing struct with one new field.

| Field | Type | Default | New? | Description |
|-------|------|---------|------|-------------|
| `rpc_port` | `uint16_t` | 12345 | No | JSON-RPC listen port |
| `p2p_port` | `uint16_t` | 12346 | No | P2P listen port |
| `timeout_seconds` | `uint32_t` | 30 | No | Network timeout |
| `log_level` | `std::string` | `"info"` | **Yes** | Log verbosity level |

**Config.json representation**:
```json
{
  "network": {
    "rpc_port": 12345,
    "p2p_port": 12346,
    "timeout_seconds": 30,
    "log_level": "info"
  }
}
```

---

## State Transitions

### Configuration Precedence Chain

```
CLI flags ──override──> config.json values ──override──> built-in defaults
```

Flow at startup:
1. Parse CLI arguments → `CliOptions`
2. Determine config path (CLI `--config` or `<blockchain_dir>/config.json`)
3. Load config file → `NodeConfig` (with built-in defaults for missing keys)
4. Apply CLI overrides onto `NodeConfig` fields
5. Validate merged `NodeConfig`
6. Set global log level from final `NodeConfig.network.log_level`
7. Proceed to normal daemon startup

### Early-Exit Commands

| Flag | Behavior | Exit Code |
|------|----------|-----------|
| `--help` / `-h` | Print help text, exit | 0 |
| `--version` / `-v` | Print version, exit | 0 |
| `--generate-config` | Write config.json + config.README, exit | 0 (success) or 1 (file exists) |

These are checked before config file loading. Only `blockchain_dir` is required for `--generate-config`.

## Known Key Registry (for FR-013)

Top-level known sections: `tls`, `network`, `consensus`, `peers`, `streams`, `persistence`.

Per-section known keys:
- **tls**: `cert_file`, `key_file`, `ca_file`
- **network**: `rpc_port`, `p2p_port`, `timeout_seconds`, `log_level`
- **consensus**: `target_block_interval`, `adjustment_window`, `max_adjustment_factor`, `min_difficulty`, `max_difficulty`, `initial_difficulty`, `mining_timeout`, `max_future_timestamp`, `max_reorg_depth`
- **peers**: `seed_nodes`, `max_outbound`, `max_inbound`, `exchange_interval_seconds`, `discovery_enabled`, `max_stored_peers`, `reconnect_base_delay_seconds`, `reconnect_max_delay_seconds`, `ban_threshold_errors`, `ban_duration_seconds`
- **streams**: `allowed_streams`
- **persistence**: `save_interval_seconds`

Any key not in these sets triggers a warning log.
