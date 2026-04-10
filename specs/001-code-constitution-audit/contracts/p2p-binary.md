# P2P Binary Protocol Contract

**Protocol**: Custom binary over TLS (mutual verification)
**Port**: 12346 (default, RPC port + 1)
**Transport**: TCP + mutual TLS

## Authentication

- **Mutual TLS**: Both server and connecting peer must present valid certificates
- Server verifies peer certificate against configured CA (`BLOCKCHAIN_CA_FILE`)
- Connection refused if peer certificate is missing or invalid

## Packet Format

### Header (16 bytes, fixed)

```
+----------+----------+
| length   | type     |
| uint64_t | uint64_t |
+----------+----------+
```

| Field | Size | Description |
|-------|------|-------------|
| `length` | 8 bytes | Size of body payload in bytes |
| `type` | 8 bytes | Packet type enum value |

### Packet Types

| Value | Name | Description |
|-------|------|-------------|
| 0 | `BLOCK` | Serialized Block (Boost.Serialization binary archive) |
| 1 | `BLOCKCHAIN_QUERY` | Request blockchain state (not yet implemented) |
| 2 | `BLOCKCHAIN_RESPONSE` | Blockchain state response (not yet implemented) |

### Body

Body format depends on `type`:

- **BLOCK**: Boost.Serialization binary archive containing a `Block` struct
- **BLOCKCHAIN_QUERY**: Reserved
- **BLOCKCHAIN_RESPONSE**: Reserved

### Server Response

After processing a valid packet, the server responds with a newline-terminated string:
```
blockchain node server\n
```

## Changes in This Audit

- **FR-004a**: Mutual TLS required — peer certificates verified
- **FR-008**: Handshake failures logged with structured output
- **FR-009**: Header read, body read, and write operations timeout after 30 seconds (configurable)
- **FR-013**: Block processing serialized via strand for thread safety
