# Data Model: Documentation & Developer Guide

**Feature**: 014-documentation-developer-guide
**Date**: 2026-04-12

## Overview

This feature produces documentation files only. There are no database entities, persistent data structures, or runtime state changes. The "data model" for this feature is the set of documentation deliverables, their content structure, and their relationships.

## Entities

### Document: README.md

**Location**: Repository root
**Purpose**: Single entry point for all project documentation

| Section | Maps to FR | Content Source |
|---------|-----------|----------------|
| Project Description | FR-001 | New prose (summarize capabilities from roadmap) |
| Build Prerequisites | FR-002 | CI workflow install steps + minimum compiler versions |
| Build Instructions (Linux) | FR-003 | CI workflow configure/build steps |
| Build Instructions (macOS) | FR-003 | CI workflow configure/build steps |
| Build Instructions (Windows) | FR-003 | CI workflow configure/build steps |
| Quickstart | FR-004 | New walkthrough (two-node setup, TLS, block propagation) |
| Documentation Links | FR-013 | Links to docs/*.md files |
| License | — | Existing MIT license reference |

### Document: docs/configuration.md

**Location**: `docs/configuration.md`
**Purpose**: Complete CLI and config.json reference

| Section | Maps to FR | Content Source |
|---------|-----------|----------------|
| CLI Flags Table | FR-005 | specs/009 CLI contract + CliParser.cpp |
| Precedence Rules | FR-005 | specs/009 CLI contract |
| config.json Schema | FR-006 | NodeConfig.cpp defaults + research.md R6 |
| TLS Setup | FR-007 | New walkthrough (OpenSSL cert generation) |

### Document: docs/rpc-api.md

**Location**: `docs/rpc-api.md`
**Purpose**: JSON-RPC method reference grouped by domain

| Section | Maps to FR | Content Source |
|---------|-----------|----------------|
| Table of Contents | FR-008 | 6 domain groups from research.md R3 |
| Streams (5 methods) | FR-008 | specs/006 + RpcServer.cpp |
| Blocks (3 methods) | FR-008 | specs/001, 010 + RpcServer.cpp |
| Peers (5 methods) | FR-008 | specs/004 + RpcServer.cpp |
| Node (3 methods) | FR-008 | specs/010 + RpcServer.cpp |
| Merkle (3 methods) | FR-008 | specs/008 + RpcServer.cpp |
| Sync (1 method) | FR-008 | specs/003 + RpcServer.cpp |
| Error Codes | FR-009 | RpcServer.cpp error handling paths |

**Method entry structure** (per method):
- H3 heading: method name (wire-format)
- Description paragraph
- Parameters table (name, type, required, description)
- Request JSON example
- Response JSON example
- `curl` example with `--cacert ca.pem`
- Error cases (if any)

### Document: docs/architecture.md

**Location**: `docs/architecture.md`
**Purpose**: High-level system overview with Mermaid diagrams

| Section | Maps to FR | Content Source |
|---------|-----------|----------------|
| System Overview diagram | FR-010 | Mermaid block diagram of 4 subsystems |
| Consensus Engine | FR-010 | ConsensusConfig.hpp, Blockchain.cpp |
| P2P Networking | FR-010 | PeerManager, PeerServer, PeerClient |
| Persistence Layer | FR-010 | Chunk, Blockchain chunk management |
| RPC Server | FR-010 | RpcServer.cpp |
| Block Mining Flow | FR-011 | Mermaid sequence diagram |
| Block Propagation Flow | FR-011 | Mermaid sequence diagram |
| Chain Sync Flow | FR-011 | Mermaid sequence diagram |
| Stream Publish Flow | FR-011 | Mermaid sequence diagram |

### Document: docs/contributing.md

**Location**: `docs/contributing.md`
**Purpose**: Contributor guide

| Section | Maps to FR | Content Source |
|---------|-----------|----------------|
| Building | FR-012 | Same as README build instructions (link back) |
| Running Tests | FR-012 | Constitution § III (individual test binaries) |
| Coding Conventions | FR-012 | Constitution § I, IV (C++20, style) |
| Pull Request Workflow | FR-012 | Constitution § VIII |
| Dependencies Policy | FR-012 | Constitution § V |

## Relationships

```
README.md
├── links to → docs/configuration.md
├── links to → docs/rpc-api.md
├── links to → docs/architecture.md
├── links to → docs/contributing.md
└── links to → docs/ROADMAP.md (existing)

docs/configuration.md
└── referenced by → README.md quickstart (for config details)

docs/rpc-api.md
└── curl examples reference → TLS setup in docs/configuration.md

docs/architecture.md
└── standalone (no outbound cross-references required)

docs/contributing.md
└── links to → README.md build instructions (avoid duplication)
```

## Validation Rules

- Every CLI flag in CliParser.cpp must appear in docs/configuration.md
- Every config.json field in NodeConfig.cpp must appear in docs/configuration.md
- Every RPC method string in RpcServer.cpp must appear in docs/rpc-api.md
- Every docs/*.md file must be linked from README.md
- All curl examples must include `--cacert ca.pem`
- All Mermaid diagrams must render on GitHub (```mermaid fenced blocks)

## State Transitions

N/A — documentation files are static artifacts with no runtime state.
