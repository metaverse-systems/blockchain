# Module Interface Contract: DifficultyEngine

**Module**: `DifficultyEngine`  
**Header**: `src/DifficultyEngine.hpp`  
**Implementation**: `src/DifficultyEngine.cpp`

## Construction

```
DifficultyEngine() = default;
```

Stateless — no owned fields. All data passed per call.

## Public Methods

```
uint32_t calculateNewDifficulty(const ConsensusConfig& config,
                                 size_t totalBlockCount,
                                 uint32_t currentDifficulty,
                                 std::function<Block(size_t)> getBlock)
```
Computes the next difficulty based on the most recent adjustment window.

**Parameters**:
- `config`: Consensus parameters (adjustmentWindow, targetBlockInterval, maxAdjustmentFactor, minDifficulty, maxDifficulty)
- `totalBlockCount`: Current total blocks in chain
- `currentDifficulty`: Current PoW difficulty
- `getBlock`: Callback to fetch a block by index (isolates from persistence)

**Returns**: New difficulty value, clamped to `[minDifficulty, maxDifficulty]`.

**Behavior**:
- If `totalBlockCount <= adjustmentWindow`: returns `currentDifficulty` unchanged
- Computes actual vs expected time ratio over the last window
- Applies log₂ adjustment, clamped by `maxAdjustmentFactor`

---

```
uint32_t getDifficultyForHeight(size_t height,
                                 const ConsensusConfig& config,
                                 size_t totalBlockCount,
                                 std::unordered_map<size_t, uint32_t>& difficultyCache,
                                 std::function<Block(size_t)> getBlock,
                                 std::function<void(size_t)> retainChunk)
```
Computes the difficulty that would apply at a given chain height, using cached values when available.

**Parameters**:
- `height`: Target chain height
- `config`: Consensus parameters
- `totalBlockCount`: Current total blocks
- `difficultyCache`: Mutable reference to cache (reads existing entries, writes new)
- `getBlock`: Callback to fetch block by index
- `retainChunk`: Callback to retain a chunk in memory during multi-access scan

**Returns**: Difficulty at the given height, clamped to `[minDifficulty, maxDifficulty]`.

**Behavior**:
- Walks adjustment boundaries from genesis to `height`
- Checks `difficultyCache` at each boundary; uses cached value if present
- Computes and caches missing boundaries
- Uses `retainChunk` to avoid repeated load/free cycles
