bin_PROGRAMS = blockchain
blockchain_SOURCES = main.cpp utils.cpp Block.cpp Chunk.cpp Blockchain.cpp \
    network/RpcServer.cpp network/PeerServer.cpp network/PeerClient.cpp
blockchain_LDADD = ${OPENSSL_LIBS} -lboost_serialization
blockchain_CXXFLAGS = -std=c++17 -Wall -Wextra -pedantic -O3 ${OPENSSL_CFLAGS} $(BOOST_CPPFLAGS)