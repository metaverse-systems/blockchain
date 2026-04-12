# blockchain Development Guidelines

Auto-generated from all feature plans. Last updated: 2026-04-12

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
- C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (EVP SHA-256, X.509 cert generation), nlohmann/json (vendored `src/json.hpp`), Catch2 (test framework) (012-integration-test-suite)
- Boost.Serialization binary chunk files in temporary directories (cleaned per test) (012-integration-test-suite)
- YAML (GitHub Actions workflow), C++20 (project under test) + GitHub Actions, MSYS2 (Windows), apt (Linux), Homebrew (macOS) (013-ci-cd-pipeline)
- N/A (CI configuration only, no persistent storage) (013-ci-cd-pipeline)
- GitHub-Flavored Markdown (documentation-only feature; no C++ changes) + N/A (Markdown files only; Mermaid diagrams rendered natively by GitHub) (014-documentation-developer-guide)
- N/A (no persistence changes) (014-documentation-developer-guide)
- C++20 (`-std=c++20`) + Boost (Asio, Serialization, Program Options), OpenSSL, nlohmann/json (vendored), Catch2 (test) (015-compile-time-optimization)
- N/A (build-system-only change) (015-compile-time-optimization)
- C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`) (016-audit-remediation)
- Boost.Serialization binary chunk files (`chunk_NNNNNN.dat`), keys (`keys.dat`), streams (`streams.dat`, `stream_index.dat`) (016-audit-remediation)

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
- 017-blockchain-module-split: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`)
- 016-audit-remediation: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization), OpenSSL (EVP SHA-256), nlohmann/json (vendored `src/json.hpp`)
- 015-compile-time-optimization: Added C++20 (`-std=c++20`) + Boost (Asio, Serialization, Program Options), OpenSSL, nlohmann/json (vendored), Catch2 (test)


<!-- MANUAL ADDITIONS START -->
<!-- MANUAL ADDITIONS END -->
