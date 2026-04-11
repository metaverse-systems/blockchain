# blockchain Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-04-11

## Active Technologies
- C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`) (002-consensus-mechanism)
- Binary chunk files via Boost.Serialization (existing) (002-consensus-mechanism)
- `config.json` (operator configuration, replaces `.env`) and `peers.json` (runtime peer state + node UUID) in blockchain data directory; Boost.Serialization binary archives for P2P wire format (004-peer-discovery)
- Boost.Serialization binary chunk files (existing) (005-block-propagation)
- Boost.Serialization binary archives for chunk files (`chunk_NNNNNN.dat`), keys (`keys.dat`), streams (`streams.dat`, `stream_index.dat`) in the blockchain data directory (007-multi-chunk-persistence)
- Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`) (008-merkle-block-headers)
- C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`), getopt (POSIX, header-only) (009-cli-configuration)
- `config.json` and `config.README` in blockchain data directory; Boost.Serialization binary archives for chain data (009-cli-configuration)

- C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored `src/json.hpp`), Catch2 (test only) (001-code-constitution-audit)

## Project Structure

```text
src/
tests/
```

## Commands

# Add commands for C++20 (`-std=c++20`)

## Code Style

C++20 (`-std=c++20`): Follow standard conventions

- Do not include task numbers (e.g. T001, T010) in code comments. Comments should describe *what* or *why*, not reference planning artifacts.

## Recent Changes
- 009-cli-configuration: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`), getopt (POSIX, header-only)
- 008-merkle-block-headers: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`)
- 008-merkle-block-headers: Added [if applicable, e.g., PostgreSQL, CoreData, files or N/A]


<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
