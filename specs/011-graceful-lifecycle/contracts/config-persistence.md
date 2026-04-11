# Contract: config.json Persistence Section

**Date**: 2026-04-11
**Feature**: 011-graceful-lifecycle

## Overview

The `persistence` section of `config.json` controls chunk save behavior and startup validation. This feature adds the `fast_startup` key.

## Schema

```json
{
  "persistence": {
    "save_interval_seconds": <uint32>,
    "fast_startup": <bool>
  }
}
```

## Fields

| Key | Type | Default | Required | Description |
|-----|------|---------|----------|-------------|
| `save_interval_seconds` | uint32 | 300 | No | Interval in seconds between periodic saves. 0 disables periodic saves. (Existing) |
| `fast_startup` | bool | false | No | When true, skip chunk integrity validation and cross-chunk linkage checks on startup. Chunks are loaded based on sequential file discovery only. |

## Validation Rules

- `save_interval_seconds`: Must be a non-negative integer. (Existing behavior)
- `fast_startup`: Must be a boolean (`true` or `false`). Unknown keys in the `persistence` section produce a warning at startup. (Follows existing validation pattern)

## Backward Compatibility

- Omitting `fast_startup` defaults to `false` (validation enabled). Existing config files work without changes.
- No existing keys are modified or removed.

## config.README Addition

```text
## persistence
  save_interval_seconds  (uint32)  Auto-save interval              default: 300
  fast_startup           (bool)    Skip chunk validation on start   default: false
```
