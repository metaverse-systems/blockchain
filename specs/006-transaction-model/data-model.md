# Data Model: 006 — Transaction Model (Stream-Based Key/Value Store)

**Date**: 2026-04-10  
**Feature**: [spec.md](spec.md)

## Entities

### StreamEntry

A single data record published to a named stream.

| Field | Type | Constraints | Description |
|-------|------|-------------|-------------|
| stream | `std::string` | Required, non-empty, max 256 chars, alphanumeric + hyphens + underscores | Stream name this entry belongs to |
| key | `std::string` | Required, non-empty | User-provided lookup key |
| data | `std::string` | Max 128 MB (134,217,728 bytes) | Opaque data payload (any text; binary via client-side base64) |

**Serialization**: Boost.Serialization (binary archive), same as all existing wire/persistence formats.

```cpp
struct StreamEntry {
    std::string stream;
    std::string key;
    std::string data;

    friend class boost::serialization::access;
    template<class Archive>
    void serialize(Archive &ar, const unsigned int /*version*/) {
        ar & stream;
        ar & key;
        ar & data;
    }
};
```

### Block (updated)

The `data` field is replaced by `entries`. No serialization versioning needed (no existing blockchains).

| Field | Type | Description |
|-------|------|-------------|
| index | `size_t` | Block sequence number |
| timestamp | `uint64_t` | Unix timestamp |
| prevHash | `std::string` | Hash of previous block |
| hash | `std::string` | This block's hash |
| nonce | `uint64_t` | PoW nonce |
| difficulty | `uint32_t` | PoW difficulty |
| entries | `std::vector<StreamEntry>` | Stream entries for this block |

**Serialization** (version 0, no versioning):

```cpp
template<class Archive>
unsigned int serialize(Archive& ar, const unsigned int version)
{
    ar & index;
    ar & timestamp;
    ar & prevHash;
    ar & hash;
    ar & nonce;
    ar & difficulty;
    ar & entries;
    return version;
}
```

**Hash calculation**: Updated to include serialized entries in the hash input so stream data is tamper-evident.

### Stream Registry (in-memory + persisted)

Tracks known stream names for `listStreams` and duplicate-creation prevention.

| Field | Type | Description |
|-------|------|-------------|
| streams | `std::set<std::string>` | Set of all known stream names |

Persisted to `streams.dat` in the blockchain data directory via Boost.Serialization binary archive. Loaded at construction time (same pattern as `loadKeys()`).

### Stream Index (in-memory + persisted)

Maps stream + key to block indices for efficient query.

| Field | Type | Description |
|-------|------|-------------|
| streamKeyIndex | `std::map<std::string, std::map<std::string, std::vector<size_t>>>` | `stream → key → [block indices]` |

Persisted to `stream_index.dat` in the blockchain data directory via Boost.Serialization binary archive. Loaded at construction time (same pattern as `loadKeys()`).

## Relationships

```
Block 1──* StreamEntry     (a block contains zero or more stream entries)
Stream 1──* StreamEntry    (a stream groups entries by name)
StreamEntry *──1 Key       (each entry has exactly one key; keys are not unique across entries)
```

## Validation Rules

| Rule | Scope | Description |
|------|-------|-------------|
| Stream name format | RPC + P2P | Must match `^[a-zA-Z0-9_-]{1,256}$` |
| Key non-empty | RPC + P2P | Key must be a non-empty string |
| Data size limit | RPC + P2P | `data.size() <= 128 * 1024 * 1024` |
| Stream permission | RPC only | If `allowed_streams` configured, stream must be in the list |
| Duplicate stream creation | RPC only | `createStream` rejects if stream already exists |

## State Transitions

Stream entries are **append-only**. No updates or deletes.

```
[No Stream] ──(createStream / first publish)──> [Stream Exists]
[Stream Exists] ──(publish)──> [Stream Exists + new entry appended]
```

## Configuration Extension

`config.json` gains a `streams` section:

```json
{
  "streams": {
    "allowed_streams": []
  }
}
```

- Empty array (default): all streams allowed
- Non-empty array: only listed streams accept local RPC publishes
