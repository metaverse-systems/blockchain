# Quickstart: Documentation & Developer Guide

**Feature**: 014-documentation-developer-guide
**Date**: 2026-04-12

## Prerequisites

- The project builds successfully on the current platform (Linux, macOS, or Windows/MSYS2)
- OpenSSL CLI available (for TLS certificate generation)
- A text editor for reviewing documentation files

## Quick Verification Steps

### 1. Verify deliverable files exist

After implementation, the following files should be present:

```
README.md                  # Expanded (was single heading)
docs/architecture.md       # NEW
docs/configuration.md      # NEW
docs/contributing.md       # NEW
docs/rpc-api.md            # NEW
docs/ROADMAP.md            # Existing (unchanged)
```

### 2. Verify README.md structure

Open `README.md` and confirm it contains:
- [ ] Project description (what the blockchain node does)
- [ ] Build prerequisites for all three platforms
- [ ] Build instructions for Linux, macOS, Windows
- [ ] Quickstart walkthrough (two-node network setup)
- [ ] Links to all docs/*.md files

### 3. Verify configuration reference completeness

Open `docs/configuration.md` and cross-check against source:

```bash
# Count CLI flags in source (should match documented flags)
grep -c 'add_options\|("' src/CliParser.cpp

# Count config.json sections in source (should match documented sections)
grep -c '"tls"\|"network"\|"consensus"\|"peers"\|"streams"\|"persistence"' src/NodeConfig.cpp
```

### 4. Verify RPC API reference completeness

```bash
# Count RPC method registrations in source
grep -c '"publish"\|"createStream"\|"listStreams"\|"getStreamEntries"\|"getStreamEntry"\|"requestSync"\|"getBlockByIndex"\|"getBlocksByKeys"\|"addPeer"\|"removePeer"\|"listPeers"\|"banPeer"\|"unbanPeer"\|"getInclusionProof"\|"verifyInclusionProof"\|"getBlockHeader"\|"getNodeStatus"\|"getBlockRange"\|"getChainLength"\|"getChunkCount"' src/network/RpcServer.cpp
```

Result should be 20 (one per method). Each must have a corresponding section in `docs/rpc-api.md`.

### 5. Verify all curl examples use TLS

```bash
# Every curl example should reference --cacert
grep -c '\-\-cacert' docs/rpc-api.md
# Should be >= 20 (at least one per method)

# No insecure flags
grep -c '\-k\|--insecure' docs/rpc-api.md
# Should be 0
```

### 6. Verify Mermaid diagrams render

Open `docs/architecture.md` on GitHub (or in a Mermaid-compatible Markdown viewer) and confirm:
- [ ] System overview diagram renders (subsystem boxes)
- [ ] Block mining sequence diagram renders
- [ ] Block propagation sequence diagram renders
- [ ] At least one additional data flow diagram renders

### 7. Verify README links

```bash
# All doc links should point to existing files
grep -oP '\(docs/[^)]+\)' README.md | tr -d '()' | while read f; do
  test -f "$f" && echo "OK: $f" || echo "MISSING: $f"
done
```

### 8. Follow the quickstart end-to-end

The most important validation: follow the README quickstart section from scratch on a clean build, starting from compiled binary through two running connected nodes.

## Expected Outcome

A developer cloning the repository for the first time can:
1. Build the project by following README build instructions
2. Set up and run a two-node network by following the quickstart
3. Look up any RPC method, CLI flag, or config field in the reference docs
4. Understand the architecture by reading the overview with diagrams
5. Know how to contribute by reading the contributing guide
