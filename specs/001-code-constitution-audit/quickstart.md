# Quickstart: Code Constitution Audit & Remediation

**Feature Branch**: `001-code-constitution-audit`

## Prerequisites

- C++20-compatible compiler (GCC 10+, Clang 10+, MSVC 19.29+)
- Boost (Asio, Serialization) ≥ 1.50
- OpenSSL
- Catch2 (for tests)
- GNU Autotools (autoconf, automake)

## Build

```bash
git checkout 001-code-constitution-audit
./autogen.sh        # If configure doesn't exist
./configure         # Will fail if compiler lacks C++20 support
make
```

## Configure TLS

Create a `.env` file in the project root (or set environment variables):

```bash
# .env
BLOCKCHAIN_CERT_FILE=/path/to/server-cert.pem
BLOCKCHAIN_KEY_FILE=/path/to/server-key.pem
BLOCKCHAIN_CA_FILE=/path/to/ca-cert.pem    # Required for P2P mutual TLS
BLOCKCHAIN_TIMEOUT=30                       # Optional, seconds
```

For development with self-signed certificates:

```bash
# Generate CA
openssl req -x509 -newkey rsa:4096 -keyout ca-key.pem -out ca-cert.pem -days 365 -nodes -subj "/CN=blockchain-ca"

# Generate server cert signed by CA
openssl req -newkey rsa:4096 -keyout server-key.pem -out server-csr.pem -nodes -subj "/CN=localhost"
openssl x509 -req -in server-csr.pem -CA ca-cert.pem -CAkey ca-key.pem -CAcreateserial -out server-cert.pem -days 365
```

## Run

```bash
./src/blockchain /path/to/blockchain/data
```

The daemon starts:
- **RPC server** on port 12345 (server-only TLS)
- **P2P server** on port 12346 (mutual TLS)

## Test

```bash
make check
```

Runs all Catch2 tests including:
- Block construction and hashing
- Server construction
- addBlock data integrity (new)
- TLS handshake (new)
- Timeout behavior (new)

## Verify RPC

```bash
# Test addBlock via JSON-RPC over TLS
echo '{"jsonrpc":"2.0","id":"1","method":"addBlock","params":{"data":"hello","keys":["test"]}}' | \
  openssl s_client -connect localhost:12345 -quiet
```

## Verify Structured Logging

When a connection fails, you should see output like:

```
[2026-04-10 12:00:00] [ERROR] SSL handshake failed: certificate verify failed
[2026-04-10 12:00:30] [WARN] Connection timed out after 30s, closing
```
