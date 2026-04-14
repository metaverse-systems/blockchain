noinst_LIBRARIES = libblockchain_core.a
libblockchain_core_a_SOURCES = Block.cpp Blockchain.cpp Chunk.cpp utils.cpp NodeConfig.cpp PeerManager.cpp \
    BlockPropagation.cpp MerkleTree.cpp ChainPersistence.cpp DifficultyEngine.cpp MerkleProofService.cpp \
    ChainService.cpp \
    network/RpcServer.cpp network/PeerServer.cpp network/PeerClient.cpp
libblockchain_core_a_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic $(BOOST_CPPFLAGS) ${OPENSSL_CFLAGS}

bin_PROGRAMS = blockchain
blockchain_SOURCES = main.cpp CliParser.cpp
blockchain_LDADD = libblockchain_core.a ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) $(BOOST_PROGRAM_OPTIONS_LIB) $(PLATFORM_LIBS)
blockchain_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -O3 ${OPENSSL_CFLAGS} $(BOOST_CPPFLAGS)
blockchain_LDFLAGS = $(BOOST_LDFLAGS)