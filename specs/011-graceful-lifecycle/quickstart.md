# Quickstart: Graceful Multi-Chunk Shutdown & Startup

**Date**: 2026-04-11
**Feature**: 011-graceful-lifecycle

## What Changed

The blockchain daemon now correctly persists and restores all chunk data across shutdown/restart cycles. Previously, only the active (last) chunk was saved on shutdown and loaded on startup, causing data loss for multi-chunk chains.

## Key Behaviors

### Shutdown
1. On SIGINT/SIGTERM, block ingestion is frozen (no new blocks accepted).
2. All chunks with unsaved modifications are persisted to disk.
3. Clean (unchanged) chunks are skipped — no unnecessary I/O.
4. Index files (keys, streams, stream index) and peer state are saved.
5. If any chunk save fails, the error is logged and remaining chunks still get saved.

### Startup
1. All chunk files are discovered by scanning for `chunk_NNNNNN.dat` files.
2. Each chunk is validated (deserialization + cross-chunk linkage).
3. If a chunk fails validation, the node operates with the longest valid prefix.
4. Only the active (last) chunk is kept in memory; historical chunks use placeholders.
5. Historical chunks are loaded on demand when blocks from them are accessed.

### Fast Startup
To skip chunk validation for trusted environments (e.g., after a clean shutdown):

```json
{
  "persistence": {
    "fast_startup": true
  }
}
```

This loads chunks by sequential file discovery without validating their integrity — faster but assumes chunk files are correct.

## Verification

After upgrading, verify the feature works:

1. Start the node and add blocks spanning at least 2 chunks (200+ blocks).
2. Send SIGTERM to the node.
3. Restart the node against the same data directory.
4. Query `getChainLength` via RPC — the count should match the pre-shutdown value.
5. Query a block from the first chunk via `getBlockRange` — it should be accessible.
