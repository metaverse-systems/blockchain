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
- N/A (read-only endpoints; no new persistence) (010-rpc-api-expansion)

- C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored `src/json.hpp`), Catch2 (test only) (001-code-constitution-audit)

## Project Structure

```text
src/
tests/
```

## Commands

- Always use `make -j8` for all build invocations (e.g., `make -j8`, `make -j8 tests/lifecycle_tests`)
- When running tests, execute each test binary individually instead of `make check`:
  ```bash
  ./tests/blockchain_tests
  ./tests/lifecycle_tests
  # ... etc.
  ```

## Code Style

C++20 (`-std=c++20`): Follow standard conventions

- Do not include task numbers (e.g. T001, T010) in code comments. Comments should describe *what* or *why*, not reference planning artifacts.

## Recent Changes
- 011-graceful-lifecycle: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`)
- 010-rpc-api-expansion: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL, nlohmann/json (vendored `src/json.hpp`)
- 009-cli-configuration: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (SHA-256 via EVP), nlohmann/json (vendored `src/json.hpp`), getopt (POSIX, header-only)


<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
