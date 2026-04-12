# Data Model: CI/CD Pipeline

**Date**: 2026-04-11

This feature introduces no runtime data entities, database tables, or persistent state. The "data model" is the structure of the GitHub Actions workflow configuration.

## Workflow Structure

### Entity: Workflow (`ci.yml`)

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Display name in GitHub Actions UI |
| `on` | trigger config | Push to `main` + pull request events |
| `jobs` | map | Collection of matrix jobs |

### Entity: Job (matrix entry)

| Field | Type | Description |
|-------|------|-------------|
| `name` | string | Matrix entry display name (e.g., "Linux GCC") |
| `os` | string | Runner image (`ubuntu-latest`, `macos-latest`, `windows-latest`) |
| `cc` | string | C compiler binary name |
| `cxx` | string | C++ compiler binary name |

### Entity: Build Matrix

| Configuration | OS | CC | CXX |
|--------------|----|----|-----|
| Linux GCC | ubuntu-latest | gcc-14 | g++-14 |
| Linux Clang | ubuntu-latest | clang-18 | clang++-18 |
| macOS Clang | macos-latest | clang | clang++ |
| Windows MinGW-w64 | windows-latest | gcc | g++ |

### Entity: Test Binary Set

14 test binaries built by `make -j8` and executed individually:

| Binary | Type |
|--------|------|
| `blockchain_tests` | Unit |
| `block_propagation_tests` | Unit |
| `block_propagation_integration_tests` | Integration |
| `chunk_persistence_tests` | Unit |
| `chunk_recovery_tests` | Unit |
| `chunk_replace_tests` | Unit |
| `merkle_tests` | Unit |
| `merkle_rpc_integration_tests` | Integration |
| `cli_tests` | Unit |
| `rpc_expansion_tests` | Unit |
| `lifecycle_tests` | Unit |
| `lifecycle_integration_tests` | Integration |
| `rpc_integration_tests` | Integration |
| `p2p_sync_integration_tests` | Integration |

## State Transitions

N/A — the workflow is stateless. Each CI run is independent.

## Validation Rules

- Workflow triggers only on pushes to `main` and pull requests.
- Each matrix entry must produce a successful build before test steps run.
- Each test binary must exit with code 0 to pass.
- All matrix jobs must pass for the overall workflow to report success.
