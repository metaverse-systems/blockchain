bin_PROGRAMS = blockchain
blockchain_SOURCES = main.cpp utils.cpp Block.cpp Chunk.cpp Blockchain.cpp NodeConfig.cpp PeerManager.cpp \
    BlockPropagation.cpp MerkleTree.cpp CliParser.cpp \
    network/RpcServer.cpp network/PeerServer.cpp network/PeerClient.cpp
blockchain_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) $(BOOST_PROGRAM_OPTIONS_LIB) $(PLATFORM_LIBS)
blockchain_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -O3 ${OPENSSL_CFLAGS} $(BOOST_CPPFLAGS)
blockchain_LDFLAGS = $(BOOST_LDFLAGS)