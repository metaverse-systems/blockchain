# Quickstart: Peer Discovery & Management

**Feature**: 004-peer-discovery
**Date**: 2026-04-10

## Prerequisites

- Three blockchain nodes built from `004-peer-discovery` branch
- TLS certificates configured for each node (cert, key, CA)
- Three separate blockchain data directories (e.g., `./node-a/`, `./node-b/`, `./node-c/`)

## Build

```bash
./autogen.sh
./configure
make
make check
```

## Scenario 1: Seed Node Bootstrap (Two Nodes)

### 1. Create config.json for Node A (seed)

```bash
mkdir -p ./node-a
cat > ./node-a/config.json << 'EOF'
{
  "tls": {
    "cert_file": "cert.pem",
    "key_file": "key.pem",
    "ca_file": "ca.pem"
  },
  "network": {
    "rpc_port": 12345,
    "p2p_port": 12346
  },
  "consensus": {},
  "peers": {
    "seed_nodes": [],
    "discovery_enabled": true
  }
}
EOF
```

### 2. Start Node A

```bash
./src/blockchain ./node-a/
```

Node A starts with no seeds (it is the seed). It generates a UUID in `node-a/peers.json` and listens for connections.

### 3. Create config.json for Node B (joiner)

```bash
mkdir -p ./node-b
cat > ./node-b/config.json << 'EOF'
{
  "tls": {
    "cert_file": "cert.pem",
    "key_file": "key.pem",
    "ca_file": "ca.pem"
  },
  "network": {
    "rpc_port": 22345,
    "p2p_port": 22346
  },
  "consensus": {},
  "peers": {
    "seed_nodes": [
      {"host": "127.0.0.1", "port": 12346}
    ],
    "discovery_enabled": true
  }
}
EOF
```

### 4. Start Node B

```bash
./src/blockchain ./node-b/
```

**Expected behavior**:
- Node B connects to Node A (seed) over TLS
- Both nodes exchange `PEER_EXCHANGE` messages (including UUIDs)
- Node B's `peers.json` now contains Node A
- Node A's `peers.json` now contains Node B
- Both log the peer exchange

### 5. Verify via RPC

```bash
# List peers on Node A
echo '{"jsonrpc":"2.0","id":"1","method":"listPeers","params":{}}' | \
  openssl s_client -connect localhost:12345 -quiet

# List peers on Node B
echo '{"jsonrpc":"2.0","id":"1","method":"listPeers","params":{}}' | \
  openssl s_client -connect localhost:22345 -quiet
```

Both should show one connected peer.

## Scenario 2: Three-Node Gossip Discovery

### 1. Start Node A and Node B as above

### 2. Create config.json for Node C (knows only Node B)

```bash
mkdir -p ./node-c
cat > ./node-c/config.json << 'EOF'
{
  "tls": {
    "cert_file": "cert.pem",
    "key_file": "key.pem",
    "ca_file": "ca.pem"
  },
  "network": {
    "rpc_port": 32345,
    "p2p_port": 32346
  },
  "consensus": {},
  "peers": {
    "seed_nodes": [
      {"host": "127.0.0.1", "port": 22346}
    ],
    "discovery_enabled": true
  }
}
EOF
```

### 3. Start Node C

```bash
./src/blockchain ./node-c/
```

**Expected behavior**:
- Node C connects to Node B (its seed)
- Node B shares its peer list with Node C (which includes Node A)
- Node C discovers Node A and connects automatically
- Within 60 seconds, all three nodes know about each other
- `listPeers` on any node shows two connected peers

## Scenario 3: Manual Peer Management (Discovery Disabled)

### 1. Create config.json with discovery disabled

```bash
mkdir -p ./node-manual
cat > ./node-manual/config.json << 'EOF'
{
  "tls": {
    "cert_file": "cert.pem",
    "key_file": "key.pem",
    "ca_file": "ca.pem"
  },
  "network": {
    "rpc_port": 42345,
    "p2p_port": 42346
  },
  "consensus": {},
  "peers": {
    "seed_nodes": [],
    "discovery_enabled": false
  }
}
EOF
```

### 2. Start the node

```bash
./src/blockchain ./node-manual/
```

The node starts with no connections and does not connect to any peers automatically.

### 3. Add a peer via RPC

```bash
echo '{"jsonrpc":"2.0","id":"1","method":"addPeer","params":{"host":"127.0.0.1","port":12346}}' | \
  openssl s_client -connect localhost:42345 -quiet
```

**Expected**: Node connects to the specified peer. `listPeers` shows one connected peer.

### 4. Remove the peer via RPC

```bash
echo '{"jsonrpc":"2.0","id":"2","method":"removePeer","params":{"host":"127.0.0.1","port":12346}}' | \
  openssl s_client -connect localhost:42345 -quiet
```

**Expected**: Node disconnects. `listPeers` shows empty peer list.

## Scenario 4: Ban and Unban

### 1. With two connected nodes, ban a peer

```bash
echo '{"jsonrpc":"2.0","id":"1","method":"banPeer","params":{"host":"127.0.0.1","port":12346,"duration_seconds":60}}' | \
  openssl s_client -connect localhost:22345 -quiet
```

**Expected**: Node B disconnects from Node A. `listPeers` on Node B shows Node A in the bans list. Node A's reconnection attempts to Node B are refused for 60 seconds.

### 2. Unban the peer

```bash
echo '{"jsonrpc":"2.0","id":"2","method":"unbanPeer","params":{"host":"127.0.0.1","port":12346}}' | \
  openssl s_client -connect localhost:22345 -quiet
```

**Expected**: Ban is lifted. Node A reconnnects to Node B on next retry cycle.

## Scenario 5: Reconnection After Crash

### 1. Start Nodes A and B, verify they are connected

### 2. Kill Node A (Ctrl+C or SIGKILL)

**Expected on Node B**:
- Detects disconnection
- Logs reconnection attempt with base delay (5s)
- If Node A stays down, retries with increasing delays (10s, 20s, 40s, ...)

### 3. Restart Node A

**Expected**:
- Node A loads `peers.json` and reconnects to known peers
- Node B's reconnection attempt succeeds
- Both nodes resume normal peer exchange
