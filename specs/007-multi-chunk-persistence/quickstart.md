# Quickstart: Multi-Chunk Persistence & Recovery

**Feature**: 007-multi-chunk-persistence  
**Date**: 2026-04-10

## Prerequisites

- Built blockchain daemon (`make` from repo root)
- A blockchain data directory with `config.json` (auto-generated on first run)
- TLS certificates configured per existing setup

## 1. Configure Periodic Save Interval (Optional)

Edit `config.json` in your blockchain data directory to add a `persistence` section:

```json
{
  "persistence": {
    "save_interval_seconds": 300
  }
}
```

- Set to `300` (5 minutes, the default) for typical usage.
- Set to `0` to disable periodic saves (chunks save only when full or on shutdown).
- Omit the section entirely to use defaults.

## 2. Start the Daemon

```bash
./blockchain /path/to/bc-dir
```

On startup, the daemon will:
1. Discover all existing `chunk_NNNNNN.dat` files in the data directory.
2. Validate the contiguous prefix (stops at first missing or corrupt file).
3. Load the active (latest) chunk into memory.
4. Load persisted indexes (`keys.dat`, `streams.dat`, `stream_index.dat`), rebuilding any that are missing.
5. Start the periodic save timer (if configured and > 0).
6. Log the total block count and chunk count.

## 3. Verify Multi-Chunk Persistence

Add blocks until the chain exceeds 100 blocks (one chunk). The daemon will log:

```
Saving 100 blocks to chunk 0 in /path/to/bc-dir/chunk_000000.dat
```

Each subsequent chunk is auto-saved when it fills. Check the data directory:

```bash
ls /path/to/bc-dir/chunk_*.dat
# chunk_000000.dat  chunk_000001.dat  chunk_000002.dat ...
```

## 4. Verify Restart Recovery

Stop the daemon (Ctrl+C or SIGTERM). On shutdown, the active chunk and all indexes are saved.

Restart:

```bash
./blockchain /path/to/bc-dir
```

The daemon will discover and validate all chunk files, load the active chunk, and resume operation. Verify with `getBlockByIndex` or `getChainBlockCount` via RPC.

## 5. Verify Periodic Save

With `save_interval_seconds` > 0, add a few blocks and wait for the interval to elapse. The daemon will log:

```
Saving N blocks to chunk X in /path/to/bc-dir/chunk_XXXXXX.dat
```

Kill the daemon forcibly (`kill -9 <pid>`) and restart. The blocks saved in the last periodic save should be present.

## 6. Verify Corruption Detection

To test corruption detection, stop the daemon and corrupt a chunk file:

```bash
# Truncate chunk 1 to simulate corruption
truncate -s 10 /path/to/bc-dir/chunk_000001.dat
```

Restart the daemon. It will log an error for chunk 1, load only chunk 0 (the contiguous prefix before corruption), and report discarded chunks.

## 7. Verify replaceChain Backup

When a peer offers a longer valid chain and `replaceChain` succeeds, the old chunk and index files are moved to `backups/<timestamp>/`:

```bash
ls /path/to/bc-dir/backups/
# 2026-04-10T153045Z/
ls /path/to/bc-dir/backups/2026-04-10T153045Z/
# chunk_000000.dat  chunk_000001.dat  keys.dat  streams.dat  stream_index.dat
```

Backups are never auto-deleted. Manage disk space manually.

## Validation Checklist

- [ ] Chain with 200+ blocks shuts down and restarts with zero block loss.
- [ ] Periodic save fires at the configured interval (check log output).
- [ ] Forcible kill (SIGKILL) loses at most one save interval's worth of blocks from the active chunk.
- [ ] Corrupted chunk file produces a clear error log identifying the chunk and file path.
- [ ] `getChainBlockCount` returns the correct total after recovery.
- [ ] `replaceChain` moves old files to timestamped backup directory.
- [ ] Missing `persistence` section in `config.json` uses default (300s).
- [ ] `save_interval_seconds: 0` disables periodic saving.
