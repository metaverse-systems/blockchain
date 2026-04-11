AM_CPPFLAGS = ${CATCH2_CFLAGS}

check_PROGRAMS = blockchain_tests block_propagation_tests block_propagation_integration_tests \
    chunk_persistence_tests chunk_recovery_tests chunk_replace_tests merkle_tests \
    merkle_rpc_integration_tests cli_tests
TESTS = $(check_PROGRAMS)
blockchain_tests_SOURCES = block_tests.cpp server_tests.cpp consensus_tests.cpp sync_tests.cpp \
    node_config_tests.cpp peer_manager_tests.cpp peer_discovery_tests.cpp \
    stream_entry_tests.cpp stream_tests.cpp \
    ../src/Chunk.cpp ../src/Block.cpp ../src/Blockchain.cpp ../src/utils.cpp ../src/NodeConfig.cpp ../src/PeerManager.cpp \
    ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
blockchain_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
blockchain_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}

block_propagation_tests_SOURCES = block_propagation_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
block_propagation_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
block_propagation_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}

block_propagation_integration_tests_SOURCES = block_propagation_integration_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
block_propagation_integration_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
block_propagation_integration_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}

chunk_persistence_tests_SOURCES = chunk_persistence_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
chunk_persistence_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
chunk_persistence_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}

chunk_recovery_tests_SOURCES = chunk_recovery_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
chunk_recovery_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
chunk_recovery_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}

chunk_replace_tests_SOURCES = chunk_replace_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
chunk_replace_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
chunk_replace_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}

merkle_tests_SOURCES = merkle_tests.cpp \
    ../src/Block.cpp ../src/Chunk.cpp ../src/Blockchain.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
merkle_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
merkle_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}

merkle_rpc_integration_tests_SOURCES = merkle_rpc_integration_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
merkle_rpc_integration_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
merkle_rpc_integration_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization ${CATCH2_LIBS}

cli_tests_SOURCES = cli_tests.cpp \
    ../src/CliParser.cpp ../src/utils.cpp ../src/NodeConfig.cpp ../src/Block.cpp ../src/Chunk.cpp \
    ../src/Blockchain.cpp ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
cli_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
cli_tests_LDADD = ${OPENSSL_LIBS} -lboost_serialization -lboost_program_options ${CATCH2_LIBS}