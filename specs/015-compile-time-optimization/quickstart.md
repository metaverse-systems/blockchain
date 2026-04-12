# Quickstart: Compile-Time Optimization

**Feature**: 015-compile-time-optimization

## What Changed

The build system was restructured to compile the 11 core source files (Block, Blockchain, Chunk, utils, NodeConfig, PeerManager, BlockPropagation, MerkleTree, RpcServer, PeerServer, PeerClient) only once into a shared static archive (`libblockchain_core.a`), rather than independently for each of the 14 build targets.

## Before vs After

| Metric | Before | After |
|--------|--------|-------|
| Core file compilations | 154 (11 × 14) | 11 (once) |
| Target-specific compilations | ~23 | ~23 |
| Total compilation units | ~177 | ~34 |

## How to Build

No change to the developer workflow:

```bash
# Clean build (main binary + all tests)
make -j8 check TESTS=

# Incremental rebuild
make -j8

# Run tests individually (per constitution)
./tests/blockchain_tests
./tests/lifecycle_tests
# ... etc.
```

## How to Add a New Test Binary

Before this change, adding a new test binary required listing all 11 core source files in `_SOURCES`. Now:

```makefile
# In tests/Makefile.am:
check_PROGRAMS += my_new_tests
my_new_tests_SOURCES = my_new_tests.cpp
my_new_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
my_new_tests_LDADD = ../src/libblockchain_core.a ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS} $(PLATFORM_LIBS)
my_new_tests_LDFLAGS = $(BOOST_LDFLAGS)
```

## Files Modified

- `src/Makefile.am` — adds `noinst_LIBRARIES = libblockchain_core.a`; main binary links archive
- `tests/Makefile.am` — all 13 test targets drop `../src/*.cpp` from `_SOURCES`; link `../src/libblockchain_core.a` instead
