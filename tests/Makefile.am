AM_CPPFLAGS = ${CATCH2_CFLAGS}

check_PROGRAMS = blockchain_tests
blockchain_tests_SOURCES = block_tests.cpp server_tests.cpp ../src/chunk.cpp ../src/Block.cpp ../src/blockchain.cpp ../src/utils.cpp
blockchain_tests_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
blockchain_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}