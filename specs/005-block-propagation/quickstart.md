# Quickstart: Block Propagation & Validation on Receipt

**Feature**: 005-block-propagation | **Date**: 2026-04-10

## What Changed

Block propagation is now automatic. When a node creates a block, it broadcasts the block to all connected peers over the P2P network. When a node receives a block from a peer, it validates the block against consensus rules and appends it to the local chain if valid. Valid blocks are relayed onward to other peers.

## How It Works

1. **Create a block** on any node via the `addBlock` JSON-RPC method (same as before).
2. The block is mined locally, appended to the chain, and **automatically broadcast** to all connected peers.
3. Each peer that receives the block:
   - Checks for duplicates (discards if already seen)
   - Checks per-peer rate limit (drops if exceeded)
   - Validates against consensus rules (`isValidNewBlock`)
   - Appends to local chain if valid
   - Relays to all other connected peers (excluding the sender)

## Testing Block Propagation

### Two-node test

```bash
# Terminal 1 — Start Node A
./src/blockchain /path/to/node-a-data/

# Terminal 2 — Start Node B (configured to connect to Node A)
./src/blockchain /path/to/node-b-data/

# Terminal 3 — Create a block on Node A
echo '{"jsonrpc":"2.0","id":"1","method":"addBlock","params":{"data":"hello","keys":["test"]}}' | \
  openssl s_client -connect localhost:12345 -quiet 2>/dev/null

# Terminal 2 — Observe Node B logs for:
#   "Received block #N"
#   "Block #N validated and appended"
```

### Three-node relay test

Connect Node A → Node B → Node C in a chain. Create a block on Node A. Verify Node C receives it via Node B's relay (Node C's logs show the block arrival).

## New Behaviors

| Scenario | Behavior |
|----------|----------|
| Block created locally | Broadcast to all peers |
| Valid block received from peer | Append + relay to other peers |
| Invalid block received | Reject, increment sender's error count |
| Duplicate block received | Silently discard |
| Block received during sync | Queued, processed after sync completes |
| Block with unknown predecessor | Deferred in pending pool (up to 60s) |
| Peer exceeds block rate limit | Block dropped, error count incremented |

## Configuration

No new configuration fields. Block propagation uses existing P2P connections, consensus rules, and peer management settings. Rate limiting defaults to 10 blocks/second per peer. The dedup cache holds 512 entries. The pending pool holds 64 entries.
