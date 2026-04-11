# Data Model: Graceful Multi-Chunk Shutdown & Startup

**Date**: 2026-04-11
**Feature**: 011-graceful-lifecycle

## Entities

### IChunk (modified)

Existing chunk base class. New field added:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `dirty_` | `bool` | `false` | Set when blocks are added/modified; cleared on successful `save()` or `load()` |

**State transitions**:
- `false → true`: On `push_back()`, `emplace_back()`, `resize()`, or any mutation to `blocks`
- `true → false`: On successful `save()` or `load()`
- Initial: `false` (construction and post-load)

**Methods added**:
- `bool isDirty() const` — returns `dirty_`
- `void markDirty()` — sets `dirty_ = true`
- `void clearDirty()` — sets `dirty_ = false`

### IBlockchain (modified)

Existing blockchain interface. New member added:

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `shutting_down_` | `bool` | `false` | Set on shutdown signal to freeze block ingestion |

**Methods added**:
- `bool isShuttingDown() const` — returns `shutting_down_`
- `void setShuttingDown()` — sets `shutting_down_ = true`

### Blockchain<ChunkHandler> (modified)

Template implementation. Changes:

| Aspect | Current | New |
|--------|---------|-----|
| `saveAllChunks()` | Saves only `chain.back()` | Iterates all chunks; saves those with `isDirty() == true` and `size() > 0` |
| `recoverChain()` | Always validates all chunks | Skips validation when `fast_startup` is enabled |
| `appendBlock()` | Unconditional | Checks `shutting_down_`; throws if true |
| `publish()` | Unconditional | Checks `shutting_down_`; throws if true |
| `startPeriodicSave()` | Saves only via existing `saveAllChunks()` | No change needed (saveAllChunks is fixed) |

### NodeConfig::PersistenceConfig (modified)

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `save_interval_seconds` | `uint32_t` | `300` | (existing) Periodic save interval |
| `fast_startup` | `bool` | `false` | Skip chunk validation on startup |

### config.json Schema (modified)

New key under `persistence` section:

```json
{
  "persistence": {
    "save_interval_seconds": 300,
    "fast_startup": false
  }
}
```

## Relationships

```
IBlockchain
  └── shutting_down_ (freeze flag)
  └── Blockchain<ChunkHandler>
        └── chain: vector<ChunkHandler>
              └── IChunk
                    └── dirty_ (per-chunk)
                    └── blocks: vector<Block>

NodeConfig
  └── PersistenceConfig
        └── save_interval_seconds
        └── fast_startup

Signal Handler
  ├── sets shutting_down_ = true
  ├── calls stopPeriodicSave()
  ├── calls save_peers()
  ├── calls saveAllChunks() [now dirty-aware]
  └── calls io_context.stop()
```

## Validation Rules

1. `fast_startup` must be a boolean in config.json. Non-boolean values produce a validation error at startup.
2. `save_interval_seconds` validation unchanged (existing).
3. `shutting_down_` is not part of persistent config — it is runtime-only state.
4. Per-chunk `dirty_` is not serialized — it is runtime-only tracking state.
