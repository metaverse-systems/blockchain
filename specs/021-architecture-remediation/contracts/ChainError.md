# Contract: ChainError Exception Hierarchy

**Source**: `src/ChainError.hpp`  
**Consumers**: All components that catch blockchain exceptions

## Hierarchy

```
std::runtime_error
└── ChainError              — base for all blockchain domain exceptions
    ├── ValidationError      — invalid blocks, streams, inputs, consensus violations
    ├── PersistenceError     — chunk/key/stream save/load failures, file I/O errors
    └── PeerError            — peer management operation failures
```

## Construction

All exceptions carry a descriptive `what()` message (inherited from `std::runtime_error`).

```
ChainError("message")
ValidationError("message")
PersistenceError("message")
PeerError("message")
```

## Catch Patterns

Callers may catch at the granularity they need:

- `catch (const ValidationError&)` — handle just validation failures
- `catch (const PersistenceError&)` — handle just I/O failures
- `catch (const ChainError&)` — handle all blockchain domain errors
- `catch (const std::runtime_error&)` — handle everything (existing behavior preserved)

## Migration Rules

| Current Pattern | New Pattern |
|----------------|------------|
| `throw std::runtime_error("Publish: shutting down")` | `throw ValidationError("Publish: shutting down")` |
| `throw std::runtime_error("Stream already exists")` | `throw ValidationError("Stream already exists")` |
| `throw std::runtime_error("Entry not found")` | `throw ValidationError("Entry not found")` |
| `logMessage("ERROR", ...) + continue` in `saveAllChunks` | `throw PersistenceError("Failed to save chunk N")` |
| `return false` in `PeerManager::remove_peer` | `throw PeerError("Peer not found: host:port")` |
| `return bool` in `PeerManager::add_peer` (capacity full) | `throw PeerError("Peer capacity exceeded")` |
| Silent no-op in `freeChunk` on freed chunk | Unchanged — not an error condition requiring notification |

## Preserved Behaviors

- `std::invalid_argument` in `parsePeerKey()`, `Block` constructor: Kept as-is. These are input validation at system boundaries, not domain errors.
- `std::out_of_range` in `MerkleProofService`: Kept as-is. Standard library convention for index errors.
- `nlohmann::json` exceptions: External library, unchanged.
