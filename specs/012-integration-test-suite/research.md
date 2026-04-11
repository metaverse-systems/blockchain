# Research: Integration Test Suite

**Date**: 2026-04-11
**Feature**: 012-integration-test-suite

## R1: Self-Signed TLS Certificate Generation at Test Time

### Decision
Generate self-signed X.509 certificates and RSA keys programmatically using OpenSSL's EVP/X509 C API, writing PEM files to a per-test temporary directory. Certificates are generated once per test fixture and cleaned up on destruction.

### Rationale
- `boost::asio::ssl::context` only supports file-based certificate loading (`use_certificate_chain_file`, `use_private_key_file`) — no BIO/memory option.
- Programmatic generation avoids depending on the `openssl` CLI tool being installed or on pre-existing certificate files.
- Writing to `std::filesystem::temp_directory_path()` with a unique subdirectory is portable across Linux, macOS, and Windows.

### Alternatives Considered
1. **Pre-generated test certificates checked into the repo** — Rejected: Certificates expire, won't reflect test-time environment, and violate FR-003.
2. **Shell out to `openssl` CLI** — Rejected: Adds dependency on CLI tool availability; harder to make cross-platform (Windows).
3. **In-memory BIO-based certificates** — Rejected: Boost.Asio `ssl::context` has no memory-based loader; would require using raw OpenSSL `SSL_CTX_*` calls and bypassing the Boost wrapper.

### Key API Functions
- `EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr)` → create key generation context
- `EVP_PKEY_keygen_init()` + `EVP_PKEY_CTX_set_rsa_keygen_bits(ctx, 2048)` + `EVP_PKEY_keygen()` → generate RSA-2048 key
- `X509_new()`, `X509_set_version(cert, 2)` (V3), `X509_gmtime_adj()`, `X509_set_pubkey()` → build certificate
- `X509_NAME_add_entry_by_txt()` for CN=localhost, self-issuer
- `X509_sign(cert, pkey, EVP_sha256())` → self-sign
- `PEM_write_X509()`, `PEM_write_PrivateKey()` → write PEM files

### Requirements
- OpenSSL 1.1.0+ (EVP_PKEY_CTX API). Project already links OpenSSL via `pkg-config`.
- No new dependencies introduced.

---

## R2: Dynamic Port Allocation

### Decision
Bind TCP acceptors to port 0 and read back the OS-assigned port via `acceptor.local_endpoint().port()` before passing it to test clients or peer connections.

### Rationale
- Standard POSIX and Boost.Asio pattern, works on all three target platforms.
- Eliminates port conflicts between parallel test runs, running daemon instances, or other services.
- Already used in the project's network setup pattern (bind → listen → accept loop), just with hardcoded ports.

### Alternatives Considered
1. **Fixed test ports (e.g., 19000–19999)** — Rejected: Risk of conflicts with other tests or services, FR-004 violation.
2. **Scan for available ports** — Rejected: Race condition between scan and bind; port-0 is the canonical solution.

### Implementation Pattern
```cpp
tcp::acceptor acceptor(io_context);
tcp::endpoint endpoint(tcp::v6(), 0);  // port 0 = OS-assigned
acceptor.open(endpoint.protocol());
acceptor.set_option(tcp::acceptor::reuse_address(true));
acceptor.set_option(boost::asio::ip::v6_only(false));
acceptor.bind(endpoint);
acceptor.listen();
uint16_t assigned_port = acceptor.local_endpoint().port();
```

---

## R3: In-Process Node Threading Pattern

### Decision
Run each blockchain node's `io_context.run()` on a dedicated `std::thread`. Use `io_context.stop()` for clean shutdown, joined by the thread destructor in the test fixture. Configuration, Blockchain, Server, and PeerManager objects live in the fixture alongside the thread.

### Rationale
- Clarified in spec: in-process on separate threads (not child processes).
- Single-process model enables: RAII cleanup guarantees, full stack traces on failure, deterministic port/lifecycle control, faster startup (no process spawn overhead).
- Mirrors the production `main.cpp` wiring: create objects → bind ports → `io_context.run()`.

### Alternatives Considered
1. **Child process spawning** — Rejected per spec clarification: harder to debug, cleanup requires signal management, port coordination requires IPC.
2. **Single-threaded coroutine-based** — Rejected: Would require restructuring the test flow to be async; the threaded model is simpler and matches `main.cpp`.

### Implementation Pattern
```cpp
struct NodeInstance {
    std::filesystem::path data_dir;
    boost::asio::io_context io_context;
    // ... ssl contexts, acceptors, blockchain, servers ...
    std::thread io_thread;

    void start() {
        // Bind ports, create servers, start accept loops
        io_thread = std::thread([this]{ io_context.run(); });
    }

    void stop() {
        io_context.stop();
        if (io_thread.joinable()) io_thread.join();
    }

    ~NodeInstance() { stop(); }
};
```

### Considerations
- **Thread safety**: `Blockchain<Chunk>` is not thread-safe internally, but each node has its own instance — no shared state between test nodes.
- **Port readiness**: After `listen()` the port is ready for connections before `io_context.run()` starts. Test clients can connect immediately after `start()`.
- **Cleanup ordering**: Stop `io_context` first (stops all async operations), then join thread, then destroy objects in reverse-construction order.

---

## R4: RPC Test Client Pattern

### Decision
Use a synchronous Boost.Asio TLS client that connects, sends a JSON-RPC request (newline-terminated), reads the response line, and parses JSON. This matches the existing RPC protocol: newline-delimited JSON-RPC over TLS.

### Rationale
- The RPC server reads via `async_read_until(buffer, '\n')` — so requests must be newline-terminated.
- Synchronous client in test code is simpler than async: send request → block for response → assert.
- Reuses existing Boost.Asio + OpenSSL dependencies.

### Alternatives Considered
1. **Async RPC client** — Rejected: Over-complex for test assertions; synchronous is clearer.
2. **External tool (curl, etc.)** — Rejected: Adds external dependency; curl doesn't speak raw TLS+newline JSON-RPC.

### Protocol Details (from RpcServer.cpp analysis)
- Connect via TLS to RPC port
- Send: `{"jsonrpc":"2.0","id":"1","method":"getChainLength","params":{}}\n`
- Receive: `{"jsonrpc":"2.0","id":"1","result":1}\n`
- Error format: `{"jsonrpc":"2.0","id":"1","error":{"code":-32601,"message":"..."}}\n`
- The client must disable certificate verification (self-signed certs) or add the test CA to the context.
