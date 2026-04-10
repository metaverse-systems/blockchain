# Data Model: Multi-Chunk Persistence & Recovery

**Feature**: 007-multi-chunk-persistence  
**Date**: 2026-04-10

## Entities

### Chunk (modified)

Existing entity. No new fields on `IChunk` or `Chunk`.

| Field | Type | Description |
|-------|------|-------------|
| `blocks` | `std::vector<Block>` | Ordered block storage (existing) |
| `index` | `std::size_t` | Chunk sequence number (existing) |
| `blockchainPath` | `std::filesystem::path` | Data directory (existing) |

**File naming convention**: `chunk_NNNNNN.dat` where `NNNNNN` is zero-padded to 6 digits.

**Behavior changes**:
- `save()`: Write to `.tmp` file first, then atomic rename to final path (R5).
- `load()`: Unchanged — already handles missing files gracefully.

---

### Blockchain\<ChunkHandler\> (modified)

| Field | Type | New? | Description |
|-------|------|------|-------------|
| `chain` | `std::vector<ChunkHandler>` | Existing | In-memory chunk vector |
| `dirty_` | `bool` | **NEW** | True when the active chunk has been modified since last save |
| `totalBlockCount_` | `size_t` | **NEW** | Cached total block count across all chunks (avoids scanning) |
| `chunkCount_` | `size_t` | **NEW** | Total number of chunks (on disk + active in memory) |
| `save_timer_` | `std::shared_ptr<boost::asio::steady_timer>` | **NEW** | Periodic save timer |
| `io_context_` | `boost::asio::io_context*` | **NEW** | Pointer to the event loop (for timer scheduling) |
| `save_interval_seconds_` | `uint32_t` | **NEW** | Periodic save interval from config (0 = disabled) |

**State transitions for a chunk**:

```
                 ┌──────────────┐
                 │  ON DISK     │ ← filled chunk after auto-save
                 │  (not in     │
                 │   memory)    │
                 └──────┬───────┘
                        │ on-demand load (query)
                        ▼
                 ┌──────────────┐
                 │  IN MEMORY   │ ← temporarily loaded for query
                 │  (read-only) │
                 └──────┬───────┘
                        │ free after query
                        ▼
                 ┌──────────────┐
                 │  ON DISK     │
                 │  (freed)     │
                 └──────────────┘

 Active chunk lifecycle:
                 ┌──────────────┐
                 │   ACTIVE     │ ← in memory, receiving blocks
                 │  (dirty)     │
                 └──────┬───────┘
                        │ periodic save / chunk full
                        ▼
                 ┌──────────────┐
                 │   ACTIVE     │ ← in memory, saved to disk
                 │  (clean)     │
                 └──────┬───────┘
                        │ reaches capacity → new chunk created
                        ▼
                 ┌──────────────┐
                 │  ON DISK     │ ← filled chunk, freed from memory
                 │  (immutable) │
                 └──────────────┘
```

**Key operations**:

| Operation | Triggers | Behavior |
|-----------|----------|----------|
| Auto-save on fill | Active chunk reaches `chunkSize` blocks | Save active chunk, create new empty chunk, set `dirty_ = false` |
| Periodic save | Timer fires, `dirty_ == true` | Save active chunk, set `dirty_ = false` |
| Shutdown save | SIGINT/SIGTERM | Save active chunk + all index files |
| On-demand load | `getBlockByIndex()` / `getBlocksByKeys()` for a non-active chunk | `loadChunk(i)`, serve blocks, `freeChunk(i)` |
| Startup discovery | Daemon starts | Enumerate chunk files 0..N, validate contiguous prefix, load active chunk only |
| replaceChain archive | `replaceChain()` called | Move old files to `backups/<timestamp>/`, persist new chain + indexes |

---

### NodeConfig (modified)

| Field | Type | New? | Description |
|-------|------|------|-------------|
| `persistence.save_interval_seconds` | `uint32_t` | **NEW** | Periodic save interval; default 300; 0 = disabled |

**Validation rule**: No minimum constraint (0 is valid = disabled). Any positive value is accepted.

**JSON path**: `config.json` → `"persistence"` → `"save_interval_seconds"`

```json
{
  "persistence": {
    "save_interval_seconds": 300
  }
}
```

---

### Backup Directory (new runtime entity)

Created only when `replaceChain()` is called.

| Attribute | Value |
|-----------|-------|
| Location | `<blockchainPath>/backups/<timestamp>/` |
| Timestamp format | ISO 8601 UTC: `YYYY-MM-DDTHHMMSSZ` (colons omitted for filesystem compatibility) |
| Contents | All `chunk_*.dat`, `keys.dat`, `streams.dat`, `stream_index.dat` moved from parent |
| Lifecycle | Never auto-deleted; operator manages cleanup |

---

## Relationships

```
Blockchain 1 ──── * Chunk          (ordered by index)
Chunk      1 ──── * Block          (ordered by block.index)
Chunk      1 ──── 1 Chunk File     (chunk_NNNNNN.dat on disk)
Blockchain 1 ──── 1 Active Chunk   (the latest chunk, in memory)
Blockchain 1 ──── * Backup Dir     (created on replaceChain)
NodeConfig 1 ──── 1 PersistenceConfig
```

## Validation Rules

1. **Chunk contiguity**: On startup, chunk files must form a contiguous sequence starting at 0. First gap terminates loading.
2. **Block hash integrity**: After deserializing a chunk, verify `block.calculateHash() == block.hash` for each block.
3. **Block linkage**: Verify `block[i].prevHash == block[i-1].hash` within and across chunk boundaries.
4. **Chunk completeness**: All chunks except the last (active) must contain exactly `chunkSize` blocks. The active chunk may contain 1 to `chunkSize` blocks.
5. **Config validation**: `persistence.save_interval_seconds` must be a non-negative integer (0 = disabled).
