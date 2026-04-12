AM_CPPFLAGS = ${CATCH2_CFLAGS}

check_PROGRAMS = blockchain_tests block_propagation_tests block_propagation_integration_tests \
    chunk_persistence_tests chunk_recovery_tests chunk_replace_tests merkle_tests \
    merkle_rpc_integration_tests cli_tests rpc_expansion_tests lifecycle_tests \
    lifecycle_integration_tests rpc_integration_tests p2p_sync_integration_tests
TESTS = $(check_PROGRAMS)
blockchain_tests_SOURCES = block_tests.cpp server_tests.cpp consensus_tests.cpp sync_tests.cpp \
    node_config_tests.cpp peer_manager_tests.cpp peer_discovery_tests.cpp \
    stream_entry_tests.cpp stream_tests.cpp \
    ../src/Chunk.cpp ../src/Block.cpp ../src/Blockchain.cpp ../src/utils.cpp ../src/NodeConfig.cpp ../src/PeerManager.cpp \
    ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
blockchain_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
blockchain_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
blockchain_tests_LDFLAGS = $(BOOST_LDFLAGS)

block_propagation_tests_SOURCES = block_propagation_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
block_propagation_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
block_propagation_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
block_propagation_tests_LDFLAGS = $(BOOST_LDFLAGS)

block_propagation_integration_tests_SOURCES = block_propagation_integration_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
block_propagation_integration_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
block_propagation_integration_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
block_propagation_integration_tests_LDFLAGS = $(BOOST_LDFLAGS)

chunk_persistence_tests_SOURCES = chunk_persistence_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
chunk_persistence_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
chunk_persistence_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
chunk_persistence_tests_LDFLAGS = $(BOOST_LDFLAGS)

chunk_recovery_tests_SOURCES = chunk_recovery_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
chunk_recovery_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
chunk_recovery_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
chunk_recovery_tests_LDFLAGS = $(BOOST_LDFLAGS)

chunk_replace_tests_SOURCES = chunk_replace_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
chunk_replace_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
chunk_replace_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
chunk_replace_tests_LDFLAGS = $(BOOST_LDFLAGS)

merkle_tests_SOURCES = merkle_tests.cpp \
    ../src/Block.cpp ../src/Chunk.cpp ../src/Blockchain.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
merkle_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
merkle_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
merkle_tests_LDFLAGS = $(BOOST_LDFLAGS)

merkle_rpc_integration_tests_SOURCES = merkle_rpc_integration_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
merkle_rpc_integration_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
merkle_rpc_integration_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
merkle_rpc_integration_tests_LDFLAGS = $(BOOST_LDFLAGS)

cli_tests_SOURCES = cli_tests.cpp \
    ../src/CliParser.cpp ../src/utils.cpp ../src/NodeConfig.cpp ../src/Block.cpp ../src/Chunk.cpp \
    ../src/Blockchain.cpp ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
cli_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
cli_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) $(BOOST_PROGRAM_OPTIONS_LIB) ${CATCH2_LIBS}
cli_tests_LDFLAGS = $(BOOST_LDFLAGS)

rpc_expansion_tests_SOURCES = rpc_expansion_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
rpc_expansion_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
rpc_expansion_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
rpc_expansion_tests_LDFLAGS = $(BOOST_LDFLAGS)

lifecycle_tests_SOURCES = lifecycle_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
lifecycle_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
lifecycle_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
lifecycle_tests_LDFLAGS = $(BOOST_LDFLAGS)

lifecycle_integration_tests_SOURCES = lifecycle_integration_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
lifecycle_integration_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
lifecycle_integration_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
lifecycle_integration_tests_LDFLAGS = $(BOOST_LDFLAGS)

rpc_integration_tests_SOURCES = rpc_integration_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
rpc_integration_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
rpc_integration_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
rpc_integration_tests_LDFLAGS = $(BOOST_LDFLAGS)

p2p_sync_integration_tests_SOURCES = p2p_sync_integration_tests.cpp \
    ../src/Block.cpp ../src/Blockchain.cpp ../src/Chunk.cpp ../src/utils.cpp ../src/NodeConfig.cpp \
    ../src/PeerManager.cpp ../src/BlockPropagation.cpp ../src/MerkleTree.cpp \
    ../src/network/PeerClient.cpp ../src/network/PeerServer.cpp ../src/network/RpcServer.cpp
p2p_sync_integration_tests_CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic -I. $(BOOST_CPPFLAGS)
p2p_sync_integration_tests_LDADD = ${OPENSSL_LIBS} $(BOOST_SERIALIZATION_LIB) ${CATCH2_LIBS}
p2p_sync_integration_tests_LDFLAGS = $(BOOST_LDFLAGS)