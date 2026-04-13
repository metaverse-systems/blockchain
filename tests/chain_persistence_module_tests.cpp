#include <catch2/catch_all.hpp>
#include "../src/ChainPersistence.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include "../src/MockChunk.hpp"
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/ConsensusConfig.hpp"
#include "TestHelpers.hpp"
#include <filesystem>

namespace {

std::filesystem::path create_test_dir(const std::string &name) {
    auto dir = std::filesystem::temp_directory_path() / ("chain_persist_mod_" + name);
    std::filesystem::create_directories(dir);
    return dir;
}

void cleanup_test_dir(const std::filesystem::path &dir) {
    std::filesystem::remove_all(dir);
}

Block make_test_block(size_t index, const std::string &prevHash) {
    StreamEntry e;
    e.stream = "test";
    e.key = "k" + std::to_string(index);
    e.data = "data" + std::to_string(index);

    Block b;
    b.index = index;
    b.timestamp = 1000 + index;
    b.entries = {e};
    b.prevHash = prevHash;
    b.difficulty = 0;
    b.nonce = 0;
    b.hash = b.calculateHash();
    return b;
}

} // anonymous namespace

TEST_CASE("ChainPersistence saveChunk/loadChunk round-trip", "[ChainPersistence]")
{
    auto dir = create_test_dir("save_load");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);

        // Genesis block already at index 0
        Block b1 = make_test_block(1, bc.getBlockByIndex(0).hash);
        bc.appendBlock(b1);

        // Save chunk 0
        bc.saveChunk(0);

        // Verify chunk file exists
        REQUIRE(std::filesystem::exists(dir / "chunk_000000.dat"));
    }

    // Load into a fresh persistence instance
    {
        ChainPersistence<Chunk> persistence(dir, 100);
        std::vector<Chunk> chain;
        chain.emplace_back(Chunk(0, dir));

        persistence.loadChunk(chain, 0);
        REQUIRE(chain[0].blocks.size() == 2);
        REQUIRE(chain[0].blocks[0].index == 0);
        REQUIRE(chain[0].blocks[1].index == 1);
    }

    cleanup_test_dir(dir);
}

TEST_CASE("ChainPersistence saveAllChunks clears dirty flag", "[ChainPersistence]")
{
    auto dir = create_test_dir("save_all");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<MockChunk> bc(dir, cfg);

        Block b1 = make_test_block(1, bc.getBlockByIndex(0).hash);
        bc.appendBlock(b1);

        REQUIRE(bc.isDirty() == true);
        bc.saveAllChunks();
        REQUIRE(bc.isDirty() == false);
    }

    cleanup_test_dir(dir);
}

TEST_CASE("ChainPersistence recoverChain rebuilds indexes", "[ChainPersistence]")
{
    auto dir = create_test_dir("recover");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);

        Block b1 = make_test_block(1, bc.getBlockByIndex(0).hash);
        bc.appendBlock(b1);
        bc.saveAllChunks();
    }

    // Recover into a fresh blockchain
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);
        bc.recoverChain();

        REQUIRE(bc.getChainBlockCount() == 2);
        Block recovered = bc.getBlockByIndex(1);
        REQUIRE(recovered.entries[0].key == "k1");
    }

    cleanup_test_dir(dir);
}

TEST_CASE("ChainPersistence discoverChunks counts files", "[ChainPersistence]")
{
    auto dir = create_test_dir("discover");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir, cfg);

        Block b1 = make_test_block(1, bc.getBlockByIndex(0).hash);
        bc.appendBlock(b1);
        bc.saveAllChunks();

        ChainPersistence<Chunk> persistence(dir, 100);
        REQUIRE(persistence.discoverChunks() == 1);
    }

    cleanup_test_dir(dir);
}

TEST_CASE("ChainPersistence freeChunk respects retained set", "[ChainPersistence]")
{
    auto dir = create_test_dir("free_retain");
    {
        ChainPersistence<MockChunk> persistence(dir, 100);
        std::vector<MockChunk> chain;
        chain.emplace_back(MockChunk(0, dir));
        chain[0].push_back(*(new Block(0, 0, "", {}, 0, 0)));

        std::set<size_t> retained = {0};
        persistence.freeChunk(chain, 0, retained);
        // Chunk should NOT be freed because it's retained
        REQUIRE(chain[0].blocks.size() == 1);

        std::set<size_t> empty_retained;
        persistence.freeChunk(chain, 0, empty_retained);
        // Chunk should now be freed
        REQUIRE(chain[0].blocks.empty());
    }

    cleanup_test_dir(dir);
}

TEST_CASE("ChainPersistence saveKeys/loadKeys round-trip", "[ChainPersistence]")
{
    auto dir = create_test_dir("keys_rt");
    {
        ChainPersistence<Chunk> persistence(dir, 100);

        std::map<std::string, std::vector<size_t>> keys;
        keys["alice"] = {0, 1, 2};
        keys["bob"] = {3};

        persistence.saveKeys(keys);

        std::map<std::string, std::vector<size_t>> loaded;
        persistence.loadKeys(loaded);

        REQUIRE(loaded.size() == 2);
        REQUIRE(loaded["alice"] == std::vector<size_t>{0, 1, 2});
        REQUIRE(loaded["bob"] == std::vector<size_t>{3});
    }

    cleanup_test_dir(dir);
}

TEST_CASE("ChainPersistence saveStreams/loadStreams round-trip", "[ChainPersistence]")
{
    auto dir = create_test_dir("streams_rt");
    {
        ChainPersistence<Chunk> persistence(dir, 100);

        std::set<std::string> streams = {"stream1", "stream2"};
        persistence.saveStreams(streams);

        std::set<std::string> loaded;
        persistence.loadStreams(loaded);

        REQUIRE(loaded == streams);
    }

    cleanup_test_dir(dir);
}

TEST_CASE("ChainPersistence saveStreamIndex/loadStreamIndex round-trip", "[ChainPersistence]")
{
    auto dir = create_test_dir("si_rt");
    {
        ChainPersistence<Chunk> persistence(dir, 100);

        StreamKeyIndex idx;
        idx["mystream"]["key1"] = {0, 1};
        idx["mystream"]["key2"] = {2};

        persistence.saveStreamIndex(idx);

        StreamKeyIndex loaded;
        persistence.loadStreamIndex(loaded);

        REQUIRE(loaded["mystream"]["key1"] == std::vector<size_t>{0, 1});
        REQUIRE(loaded["mystream"]["key2"] == std::vector<size_t>{2});
    }

    cleanup_test_dir(dir);
}
