# Contract: /health HTTPS Endpoint

## Overview

TLS-protected HTTPS GET endpoint for node health checking. Returns JSON with current node status.

## Request

```
GET /health HTTP/1.1
Host: localhost:9090
```

Connection is TLS-encrypted. No additional authentication. No request body, no query parameters.

## Response (Healthy)

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 123

{
  "status": "healthy",
  "chain_height": 1234,
  "peer_count": 5,
  "chunk_count": 13,
  "uptime_seconds": 3600.5,
  "last_block_index": 1234
}
```

## Response (Shutting Down)

```
HTTP/1.1 200 OK
Content-Type: application/json
Content-Length: 125

{
  "status": "shutting_down",
  "chain_height": 1234,
  "peer_count": 5,
  "chunk_count": 13,
  "uptime_seconds": 7200.0,
  "last_block_index": 1234
}
```

## Response (404 for unknown paths)

```
HTTP/1.1 404 Not Found
Content-Type: text/plain
Content-Length: 9

Not Found
```

## Field Definitions

| Field | Type | Description |
|-------|------|-------------|
| `status` | string | `"healthy"` or `"shutting_down"` |
| `chain_height` | integer | Number of blocks minus 1 (genesis is height 0) |
| `peer_count` | integer | Currently connected peer count |
| `chunk_count` | integer | Number of chunk files on disk |
| `uptime_seconds` | number | Floating-point seconds since node start |
| `last_block_index` | integer | Index of the last block in chain, -1 if empty |

## Edge Cases

- Empty chain (only genesis): `chain_height=0`, `last_block_index=-1`
- No peers: `peer_count=0`, status remains `"healthy"`
- Shutdown in progress: `status="shutting_down"`, other fields reflect current state

## Performance

- Response time: <50ms under normal load
- No blocking I/O: all values are in-memory snapshots
