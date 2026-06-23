# Contract: Structured Logging

## Overview

Log output format configuration. Supports human-readable text (default) and JSON structured output.

## Configuration

Set `network.log_format` in `config.json`:

```json
{
  "network": {
    "log_level": "info",
    "log_format": "text"
  }
}
```

Valid values for `log_format`: `"text"` (default), `"json"`

Valid values for `log_level`: `"trace"`, `"debug"`, `"info"`, `"warn"`, `"error"`

---

## Text Format (Default)

```
[2026-06-23 14:30:00] [INFO]  Node started on port 12345
[2026-06-23 14:30:01] [WARN]  Peer connection timeout: 192.168.1.1:12346
[2026-06-23 14:30:02] [ERROR] Failed to validate block 1234
```

### Format Rules

- Timestamp: `[YYYY-MM-DD HH:MM:SS]` (local time)
- Level: `[TRACE]`, `[DEBUG]`, `[INFO]`, `[WARN]`, `[ERROR]`
- ANSI color codes applied for terminal output
- Output to `stderr`

---

## JSON Format

```json
{"timestamp":"2026-06-23T14:30:00Z","level":"INFO","message":"Node started on port 12345"}
{"timestamp":"2026-06-23T14:30:01Z","level":"WARN","message":"Peer connection timeout: 192.168.1.1:12346"}
{"timestamp":"2026-06-23T14:30:02Z","level":"ERROR","message":"Failed to validate block 1234"}
```

### Format Rules

- One JSON object per line (JSON Lines format)
- `timestamp`: ISO 8601 UTC (`YYYY-MM-DDTHH:MM:SSZ`)
- `level`: Uppercase string (`"TRACE"`, `"DEBUG"`, `"INFO"`, `"WARN"`, `"ERROR"`)
- `message`: Log message text (special characters JSON-escaped)
- No ANSI color codes
- Output to `stderr`

---

## Log Level Filtering

| Config Value | Levels Shown |
|--------------|-------------|
| `"trace"` | TRACE, DEBUG, INFO, WARN, ERROR |
| `"debug"` | DEBUG, INFO, WARN, ERROR |
| `"info"` | INFO, WARN, ERROR |
| `"warn"` | WARN, ERROR |
| `"error"` | ERROR |

Messages below the configured level are filtered at macro level (no string evaluation).

---

## Edge Cases

- Special characters in messages (newlines, quotes, non-UTF-8) are properly JSON-escaped when using JSON format
- Hot-reload: Format change takes effect on next log message (no restart needed)
- Macro usage: `LOG_TRACE(msg)`, `LOG_DEBUG(msg)`, `LOG_INFO(msg)`, `LOG_WARN(msg)`, `LOG_ERROR(msg)`
