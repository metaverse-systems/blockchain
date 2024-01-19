bin_PROGRAMS = blockchain
blockchain_SOURCES = main.cpp utils.cpp block.cpp chunk.cpp blockchain.cpp \
    network/rpc_server.cpp network/p2p_server.cpp network/p2p_client.cpp
blockchain_LDADD = ${OPENSSL_LIBS} -lboost_serialization
blockchain_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3 ${OPENSSL_CFLAGS} $(BOOST_CPPFLAGS)