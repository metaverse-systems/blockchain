# Quickstart: Merkle Tree & Block Header Optimization

**Feature**: 008-merkle-block-headers  
**Date**: 2026-04-11

## Prerequisites

- C++20 compiler (g++ or clang++)
- Boost (Serialization)
- OpenSSL
- Catch2 (for tests)
- GNU Autotools

## Build

```bash
cd /path/to/blockchain
make clean && make
```

## Run Tests

```bash
make check
```

Merkle-specific tests will be in:
- `tests/merkle_tests.cpp` — Merkle tree construction, proof generation, proof verification
- `tests/block_tests.cpp` — Updated block tests covering merkleRoot in hash computation and serialization

## Verify Merkle Root in Blocks

After starting the blockchain daemon:

```bash
# Publish an entry
echo '{"jsonrpc":"2.0","id":"1","method":"publish","params":{"stream":"test","key":"k1","data":"hello"}}' | openssl s_client -connect localhost:12345 -quiet

# Response includes merkleRoot field:
# {"jsonrpc":"2.0","id":"1","result":{"index":1,"timestamp":...,"merkleRoot":"a1b2c3...","hash":"00ab...",...}}
```

## Request an Inclusion Proof

```bash
# Get proof that entry at position 0 exists in block 1
echo '{"jsonrpc":"2.0","id":"2","method":"getInclusionProof","params":{"blockIndex":1,"entryIndex":0}}' | openssl s_client -connect localhost:12345 -quiet

# Response:
# {"jsonrpc":"2.0","id":"2","result":{"blockIndex":1,"entryIndex":0,"merkleRoot":"...","leafHash":"...","proof":[...]}}
```

## Verify an Inclusion Proof

```bash
# Verify the proof from the previous step
echo '{"jsonrpc":"2.0","id":"3","method":"verifyInclusionProof","params":{"blockIndex":1,"leafHash":"<leafHash>","proof":[<proof array>]}}' | openssl s_client -connect localhost:12345 -quiet

# Response:
# {"jsonrpc":"2.0","id":"3","result":{"valid":true,"merkleRoot":"..."}}
```

## Get Block Header Only

```bash
# Get lightweight header (no entry data)
echo '{"jsonrpc":"2.0","id":"4","method":"getBlockHeader","params":{"blockIndex":1}}' | openssl s_client -connect localhost:12345 -quiet

# Response contains only header fields: index, timestamp, prevHash, merkleRoot, nonce, difficulty, hash
```

## Validation Checklist

- [ ] Blocks include `merkleRoot` field in JSON responses
- [ ] `getInclusionProof` returns valid proof for existing entries
- [ ] `getInclusionProof` returns error for out-of-range entry/block indices
- [ ] `verifyInclusionProof` returns `valid: true` for correct proofs
- [ ] `verifyInclusionProof` returns `valid: false` for tampered proofs
- [ ] `getBlockHeader` returns header-only response without entries
- [ ] All existing tests pass unchanged
- [ ] New Merkle tests pass via `make check`
