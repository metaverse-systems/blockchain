# Research: Documentation & Developer Guide

**Feature**: 014-documentation-developer-guide
**Date**: 2026-04-12
**Status**: Complete

## Research Tasks

### R1: Existing documentation sources to consolidate

**Decision**: Consolidate information from 13 spec-level quickstart.md files, 19 contract docs across specs, the CI workflow, and source code inspection into 5 user-facing documents.

**Rationale**: The information already exists scattered across spec directories but is invisible to end users. The documentation feature surfaces this information in standard locations (README.md, docs/).

**Sources identified**:
- `specs/009-cli-configuration/contracts/cli-interface.md` — CLI flags, usage, exit codes, precedence rules
- `specs/010-rpc-api-expansion/contracts/jsonrpc.md` — getNodeStatus, getBlockRange, getChainLength, getChunkCount
- `specs/008-merkle-block-headers/contracts/json-rpc.md` — getInclusionProof, verifyInclusionProof, getBlockHeader
- `specs/006-transaction-model/contracts/json-rpc.md` — publish, createStream, listStreams, getStreamEntries, getStreamEntry
- `specs/004-peer-discovery/contracts/json-rpc.md` — addPeer, removePeer, listPeers, banPeer, unbanPeer
- `specs/003-chain-sync/contracts/json-rpc.md` — requestSync
- `specs/001-code-constitution-audit/contracts/json-rpc.md` — getBlockByIndex, getBlocksByKeys
- `src/NodeConfig.cpp` — config.json schema with defaults
- `src/CliParser.cpp` — CLI flag definitions
- `.github/workflows/ci.yml` — build steps per platform, dependency install commands
- `.specify/memory/constitution.md` — coding conventions and workflow rules

### R2: Best practices for Markdown technical documentation

**Decision**: Follow standard GitHub project documentation conventions.

**Rationale**: GitHub natively renders Markdown, Mermaid diagrams, and provides table of contents via heading anchors. No external tooling required.

**Key practices**:
- README.md as single entry point with links to all sub-docs
- One topic per document (configuration, API, architecture, contributing)
- Consistent heading hierarchy (H2 for sections, H3 for subsections)
- Code blocks with language tags for syntax highlighting
- Mermaid fenced code blocks (```mermaid) for diagrams
- Anchor links for cross-referencing within and between documents

### R3: RPC API documentation format and grouping

**Decision**: Group 20 RPC methods into 6 functional domains with a table of contents.

**Rationale**: Developers look up API methods by what they want to do, not by alphabetical order or implementation history. Domain grouping matches mental models.

**Groups**:
1. **Streams** (5 methods): `publish`, `createStream`, `listStreams`, `getStreamEntries`, `getStreamEntry`
2. **Blocks** (3 methods): `getBlockByIndex`, `getBlocksByKeys`, `getBlockRange`
3. **Peers** (5 methods): `addPeer`, `removePeer`, `listPeers`, `banPeer`, `unbanPeer`
4. **Node** (3 methods): `getNodeStatus`, `getChainLength`, `getChunkCount`
5. **Merkle** (3 methods): `getInclusionProof`, `verifyInclusionProof`, `getBlockHeader`
6. **Sync** (1 method): `requestSync`

**Each method entry includes**: method name heading, description, parameters table, request JSON example, response JSON example, curl command with `--cacert ca.pem`, error cases.

### R4: TLS certificate generation for quickstart

**Decision**: Document self-signed CA + server cert generation using OpenSSL CLI in the quickstart.

**Rationale**: OpenSSL is already a project dependency. Users can generate test certificates without installing additional tools. The quickstart needs two nodes with mutual TLS, so a shared CA with two server certs is the simplest approach.

**Steps to document**:
1. Generate CA key and self-signed CA cert
2. Generate server key and CSR for node 1
3. Sign with CA, producing node 1 cert
4. Repeat for node 2
5. Place ca.pem, cert.pem, key.pem in each node's blockchain directory

### R5: Cross-platform build instructions accuracy

**Decision**: Derive build commands directly from the CI workflow (`.github/workflows/ci.yml`) to ensure accuracy.

**Rationale**: The CI pipeline runs on every PR and is the ground truth for what commands work on each platform. Commands extracted:

**Linux (Ubuntu)**:
```bash
sudo apt-get install autoconf-archive pkg-config libboost-serialization-dev libboost-program-options-dev libssl-dev catch2
autoreconf -fi
./configure
make -j8
```

**macOS**:
```bash
brew install boost openssl@3 catch2 autoconf automake autoconf-archive libtool pkg-config
export PKG_CONFIG_PATH="$(brew --prefix openssl@3)/lib/pkgconfig:$PKG_CONFIG_PATH"
export LDFLAGS="-L$(brew --prefix boost)/lib $LDFLAGS"
export CPPFLAGS="-I$(brew --prefix boost)/include $CPPFLAGS"
autoreconf -fi
./configure
make -j8
```

**Windows (MSYS2 UCRT64)**:
```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-boost mingw-w64-ucrt-x86_64-openssl mingw-w64-ucrt-x86_64-cmake autotools autoconf-archive make pkg-config git
# Build Catch2 from source (v3.4.0)
git clone --depth 1 --branch v3.4.0 https://github.com/catchorg/Catch2.git /tmp/catch2
cmake -S /tmp/catch2 -B /tmp/catch2/build -G "MSYS Makefiles" -DCMAKE_INSTALL_PREFIX=$MINGW_PREFIX
cmake --build /tmp/catch2/build -j8
cmake --install /tmp/catch2/build
# Build project
autoreconf -fi
./configure --with-boost-libdir=$MINGW_PREFIX/lib
make -j8
```

### R6: config.json complete schema documentation

**Decision**: Document all 30 config.json fields organized by section (tls, network, consensus, peers, streams, persistence).

**Rationale**: Extracted directly from `NodeConfig.cpp` load/save methods. The complete schema with defaults:

| Section | Field | Type | Default | Description |
|---------|-------|------|---------|-------------|
| tls | cert_file | string | "cert.pem" | TLS certificate file path (relative to blockchain dir) |
| tls | key_file | string | "key.pem" | TLS private key file path |
| tls | ca_file | string | "" | CA certificate for peer verification (empty = no verification) |
| network | rpc_port | uint16 | 12345 | JSON-RPC listen port |
| network | p2p_port | uint16 | 12346 | P2P listen port |
| network | timeout_seconds | uint32 | 30 | Connection timeout |
| network | log_level | string | "info" | Log verbosity: debug, info, warning, error |
| consensus | target_block_interval | uint32 | 10 | Target seconds between blocks |
| consensus | adjustment_window | uint32 | 10 | Blocks between difficulty adjustments |
| consensus | max_adjustment_factor | double | 4.0 | Max difficulty change ratio per adjustment |
| consensus | min_difficulty | uint32 | 1 | Minimum PoW difficulty (leading zero bits) |
| consensus | max_difficulty | uint32 | 16 | Maximum PoW difficulty |
| consensus | initial_difficulty | uint32 | 1 | Starting difficulty |
| consensus | mining_timeout | uint32 | 30 | Max seconds to mine one block |
| consensus | max_future_timestamp | uint32 | 120 | Max seconds a block timestamp may be in the future |
| consensus | max_reorg_depth | uint32 | 100 | Max chain reorganization depth |
| peers | seed_nodes | array | [] | Initial peers [{host, port}] |
| peers | max_outbound | uint32 | 8 | Max outbound peer connections |
| peers | max_inbound | uint32 | 32 | Max inbound peer connections |
| peers | exchange_interval_seconds | uint32 | 30 | Peer exchange gossip interval |
| peers | discovery_enabled | bool | true | Enable automatic peer discovery |
| peers | max_stored_peers | uint32 | 256 | Max peers to persist in peers.json |
| peers | reconnect_base_delay_seconds | uint32 | 5 | Initial reconnection delay |
| peers | reconnect_max_delay_seconds | uint32 | 300 | Maximum reconnection backoff |
| peers | ban_threshold_errors | uint32 | 10 | Errors before auto-ban |
| peers | ban_duration_seconds | uint32 | 3600 | Ban duration (seconds) |
| streams | allowed_streams | array | [] | Stream names this node may publish to (empty = all) |
| persistence | save_interval_seconds | uint32 | 300 | Periodic chunk save interval |
| persistence | fast_startup | bool | false | Skip full validation on recovery |

**Alternatives considered**: Auto-generating docs from code annotations — rejected because no annotation system exists and adding one would be out of scope.
