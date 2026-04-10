AM_CPPFLAGS = ${CATCH2_CFLAGS}

check_PROGRAMS = blockchain_tests
blockchain_tests_SOURCES = block_tests.cpp server_tests.cpp consensus_tests.cpp sync_tests.cpp ../src/Chunk.cpp ../src/Block.cpp ../src/Blockchain.cpp ../src/utils.cpp
blockchain_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
blockchain_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}