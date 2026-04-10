# Quickstart: Consensus Mechanism

**Feature**: 002-consensus-mechanism

## What Changes

After this feature, the blockchain enforces Proof-of-Work consensus:
- Every new block requires a valid nonce that makes its hash start with the required number of zero bits
- The `addBlock` RPC now mines (computes the nonce) before returning
- Blocks received from peers are validated against consensus rules
- Difficulty adjusts automatically based on how fast blocks are being produced

## New Environment Variables

Add these to your blockchain directory's `.env` file (all optional — sensible defaults apply):

```bash
# Consensus parameters
BLOCKCHAIN_TARGET_INTERVAL=10       # Target seconds between blocks (default: 10)
BLOCKCHAIN_ADJUST_WINDOW=10         # Blocks between difficulty adjustments (default: 10)
BLOCKCHAIN_MAX_ADJUST_FACTOR=4.0    # Max difficulty change per adjustment (default: 4.0)
BLOCKCHAIN_MIN_DIFFICULTY=1         # Minimum difficulty in leading zero bits (default: 1)
BLOCKCHAIN_MAX_DIFFICULTY=16        # Maximum difficulty in leading zero bits (default: 16)
BLOCKCHAIN_INITIAL_DIFFICULTY=1     # Starting difficulty for new chains (default: 1)
BLOCKCHAIN_MINING_TIMEOUT=30        # Max seconds to spend mining a block (default: 30)
BLOCKCHAIN_MAX_FUTURE_TIMESTAMP=120 # Max seconds a block timestamp can be in the future (default: 120)
BLOCKCHAIN_MAX_REORG_DEPTH=100      # Max blocks to replace during chain reorganization (default: 100)
```

## Build

No new dependencies. Build as before:

```bash
./autogen.sh    # if building from git
./configure
make
make check      # run tests including new consensus tests
```

## Usage

Start the node as usual:

```bash
./src/blockchain /path/to/blockchain-dir
```

The `addBlock` RPC works the same way but now mines the block:

```bash
echo '{"jsonrpc":"2.0","id":"1","method":"addBlock","params":{"data":"hello","keys":["test"]}}' | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null
```

The response now includes `nonce` and `difficulty` fields:

```json
{
  "jsonrpc": "2.0",
  "id": "1",
  "result": {
    "index": 1,
    "timestamp": 1712764800,
    "data": "hello",
    "prevHash": "abc...",
    "hash": "0abc...",
    "nonce": 3,
    "difficulty": 1
  }
}
```

## What's Different for Existing Users

- **RPC clients**: No changes needed. The `addBlock` request format is unchanged. Responses gain `nonce` and `difficulty` fields (additive, backward compatible).
- **Mining latency**: At the default difficulty of 1 bit, mining is near-instant (~2 hash attempts on average). You won't notice any delay.

## Testing the Consensus

```bash
# Run all tests
make check

# The new consensus_tests binary covers:
# - Block mining produces valid PoW
# - Invalid PoW blocks are rejected
# - Difficulty adjustment responds to fast/slow mining
# - Chain replacement follows longest-valid-chain rule
# - Mining timeout returns error
# - Genesis block is exempt from PoW
```
