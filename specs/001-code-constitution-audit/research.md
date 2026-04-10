# Research: Code Constitution Audit & Remediation

**Feature Branch**: `001-code-constitution-audit`
**Date**: 2026-04-10
**Purpose**: Resolve all technical unknowns before Phase 1 design.

---

## R1: Autotools C++20 Enforcement

**Decision**: Add `AX_CXX_COMPILE_STDCXX(20, noext, mandatory)` to `configure.ac`.

**Rationale**: This is the standard GNU Autotools macro for compiler standard enforcement. Parameters:
- `20` — C++20 standard
- `noext` — strict `-std=c++20`, not `-std=gnu++20` (GNU extensions disabled for portability)
- `mandatory` — fail `./configure` if compiler lacks C++20 support

Additionally, update both `src/Makefile.am` and `tests/Makefile.am` to replace `-std=c++17` with `-std=c++20`.

**Alternatives considered**:
- `optional` mode — rejected because C++20 features (`std::format`, `std::filesystem` improvements) will be used
- Manual `CXXFLAGS` — rejected because autoconf macro is portable and handles compiler detection

---

## R2: Thread Safety via Boost.Asio Strand

**Decision**: Wrap Blockchain access in `boost::asio::strand` for the single `io_context` architecture.

**Rationale**: The Blockchain object is passed by reference (`IBlockchain &bc`) to all `Server<>` instances sharing the same `io_context`. A strand serializes handler execution without explicit locking, which is more natural for the async callback model already in use.

| Approach | Thread Safety | Complexity | Performance |
|----------|---------------|-----------|-------------|
| `boost::asio::strand` | Implicit serialization | Low | Minimal overhead |
| `std::mutex` | Explicit locking | Medium | Lock contention risk |
| No protection | None | N/A | Unsafe |

**Alternatives considered**:
- `std::mutex` — rejected because strand integrates naturally with Boost.Asio handlers and avoids potential deadlocks in async code
- Per-connection strand — overkill for shared-state protection

---

## R3: Async Operation Timeouts via Deadline Timer

**Decision**: Use `boost::asio::steady_timer` alongside each `async_read`/`async_write`. Default timeout: 30 seconds.

**Rationale**: Start a timer before each async operation. When the async operation completes, cancel the timer. When the timer fires first, close the socket. Both handlers check for `boost::asio::error::operation_aborted` to distinguish cancellation from timeout.

Pattern:
1. `timer.expires_after(std::chrono::seconds(30))`
2. `timer.async_wait(...)` — on expiry, close socket
3. `async_read(...)` — on completion, cancel timer
4. Whichever fires second receives `operation_aborted`

**Alternatives considered**:
- OS-level socket timeouts (`SO_RCVTIMEO`) — rejected because not portable across platforms and not integrated with Boost.Asio event loop
- No timeout — rejected because it creates a DoS vulnerability (constitution Principle X)

---

## R4: SSL Context — Mutual TLS vs Server-Only

**Decision**: Use separate SSL contexts for P2P (mutual TLS) and RPC (server-only).

**Rationale**:
- **P2P (mutual TLS)**: `ssl_context.set_verify_mode(ssl::verify_peer | ssl::verify_fail_if_no_peer_cert)` + `ssl_context.load_verify_file(ca_cert)` — both sides present and verify certificates
- **RPC (server-only)**: `ssl_context.set_verify_mode(ssl::verify_none)` on the server side — server presents its certificate, clients are not required to present one

This requires two `ssl::context` instances in `main.cpp` instead of the current single shared one.

**Alternatives considered**:
- Single context with `set_verify_callback` — rejected because it conflates two different security models
- `verify_peer` without `verify_fail_if_no_peer_cert` — rejected because it allows connections with invalid certs

---

## R5: .env File Parsing Without External Dependencies

**Decision**: Hand-written `loadDotEnv()` utility function using `std::filesystem::path` and `std::ifstream`.

**Rationale**: Simple KEY=VALUE format. Handles:
- Comments (`#` at line start)
- Empty lines (skip)
- Quoted values (strip surrounding `"` and `'`)
- Whitespace trimming around key and value
- Calls `setenv()` (POSIX) / `_putenv_s()` (Windows) to inject into process environment

Placed in `src/utils.cpp` alongside existing utility functions.

**Alternatives considered**:
- External dotenv library — rejected (Principle V: minimal dependencies)
- Environment variables only (no .env) — rejected per clarification: user explicitly requested `.env` loader

---

## R6: Structured Logging to Stderr

**Decision**: Minimal log helper in `src/utils.hpp`/`src/utils.cpp` producing `[YYYY-MM-DD HH:MM:SS] [LEVEL] message\n` to stderr.

**Rationale**: C++20 `std::chrono::system_clock::now()` for timestamps. Use `std::put_time` with `std::localtime` for formatting (portable). Severity levels: `ERROR`, `WARN`, `INFO`. No buffering — write directly to `std::cerr`.

Note: `std::format` availability varies by compiler (GCC 13+, Clang 17+, MSVC 19.29+). Using `std::put_time` via `<iomanip>` as the baseline approach for maximum C++20 compiler compatibility.

**Alternatives considered**:
- spdlog — rejected (Principle V: minimal dependencies)
- Raw `std::cerr <<` — rejected per clarification: structured output required
- `std::format` only — rejected because compiler support is not universal yet

---

## R7: Portable Path Construction

**Decision**: Replace all `blockchainPath.string() + "/..."` patterns with `blockchainPath / filename`.

**Rationale**: `std::filesystem::path::operator/` handles platform-specific path separators automatically (`/` on POSIX, `\` on Windows). This is the idiomatic C++ approach and has been standard since C++17.

Affected locations:
- `src/Chunk.cpp` lines 7 and 26: chunk file path construction
- `src/Blockchain.cpp` lines 96 and 104: keys file path construction

**Alternatives considered**:
- `path::append()` — equivalent but less idiomatic
- Manual platform detection — error-prone; `std::filesystem` handles this correctly
