# blockchain Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-04-10

## Active Technologies
- C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`) (002-consensus-mechanism)
- Binary chunk files via Boost.Serialization (existing) (002-consensus-mechanism)
- `config.json` (operator configuration, replaces `.env`) and `peers.json` (runtime peer state + node UUID) in blockchain data directory; Boost.Serialization binary archives for P2P wire format (004-peer-discovery)

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
- 004-peer-discovery: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored `src/json.hpp`)
- 003-chain-sync: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored `src/json.hpp`)
- 002-consensus-mechanism: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`)


<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
