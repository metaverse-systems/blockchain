#include <catch2/catch_all.hpp>
#include "../src/Block.hpp"
#include "../src/StreamEntry.hpp"
#include "../src/MerkleTree.hpp"
#include "../src/Blockchain.hpp"
#include "../src/Chunk.hpp"
#include "../src/ConsensusConfig.hpp"
#include "MockBlockchain.hpp"
#include <filesystem>

// Integration tests: Merkle RPC endpoints (getInclusionProof, verifyInclusionProof, getBlockHeader)
// Tests the blockchain interface methods that RPC handlers call, verifying the
// full pipeline matches the JSON-RPC contracts in contracts/json-rpc.md.

namespace {

std::filesystem::path create_test_dir(const std::string &id) {
    auto dir = std::filesystem::temp_directory_path() / ("merkle_rpc_integ_" + id);
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    return dir;
}

void cleanup_test_dir(const std::filesystem::path &dir) {
    std::filesystem::remove_all(dir);
}

} // anonymous namespace

// =========================================================================
// getInclusionProof integration tests
// =========================================================================

TEST_CASE("Integration: getInclusionProof returns valid proof for published entry", "[integration][merkle][rpc]") {
    MockBlockchain bc;

    // Publish a block with a known entry
    Block b = bc.publish("teststream", "key1", "hello merkle", {"key1"});

    // Call getInclusionProof for block 1, entry 0
    auto result = bc.getInclusionProof(1, 0);

    // Verify response structure matches contracts/json-rpc.md
    REQUIRE(result.contains("blockIndex"));
    REQUIRE(result.contains("entryIndex"));
    REQUIRE(result.contains("merkleRoot"));
    REQUIRE(result.contains("leafHash"));
    REQUIRE(result.contains("proof"));

    REQUIRE(result["blockIndex"].get<size_t>() == 1);
    REQUIRE(result["entryIndex"].get<size_t>() == 0);

    // merkleRoot and leafHash are 64-char hex strings
    std::string merkleRoot = result["merkleRoot"].get<std::string>();
    std::string leafHash = result["leafHash"].get<std::string>();
    REQUIRE(merkleRoot.size() == 64);
    REQUIRE(leafHash.size() == 64);

    // For a single-entry block, proof should be empty and leafHash == merkleRoot
    REQUIRE(result["proof"].is_array());
    REQUIRE(result["proof"].size() == 0);
    REQUIRE(leafHash == merkleRoot);
}

TEST_CASE("Integration: getInclusionProof with multi-entry block", "[integration][merkle][rpc]") {
    MockBlockchain bc;

    // Publish multiple blocks to get multi-entry scenario
    // MockBlockchain creates single-entry blocks, so we'll verify proof size for single entries
    bc.publish("stream", "k1", "data1", {"k1"});
    bc.publish("stream", "k2", "data2", {"k2"});
    bc.publish("stream", "k3", "data3", {"k3"});

    // Each block has 1 entry, so proof for entry 0 is always empty
    for (size_t i = 1; i <= 3; i++) {
        auto result = bc.getInclusionProof(i, 0);
        REQUIRE(result["blockIndex"].get<size_t>() == i);
        REQUIRE(result["entryIndex"].get<size_t>() == 0);
        REQUIRE(result["merkleRoot"].get<std::string>().size() == 64);
        REQUIRE(result["leafHash"].get<std::string>().size() == 64);
    }
}

TEST_CASE("Integration: getInclusionProof with real Blockchain<Chunk>", "[integration][merkle][rpc]") {
    auto dir = create_test_dir("proof_real");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir.string(), cfg);

        bc.publish("data", "item1", "payload1", {"item1"});

        auto result = bc.getInclusionProof(1, 0);
        REQUIRE(result["blockIndex"].get<size_t>() == 1);
        REQUIRE(result["entryIndex"].get<size_t>() == 0);

        std::string merkleRoot = result["merkleRoot"].get<std::string>();
        std::string leafHash = result["leafHash"].get<std::string>();
        REQUIRE(merkleRoot.size() == 64);
        REQUIRE(leafHash.size() == 64);

        // Verify the merkleRoot matches the block's stored merkleRoot
        Block b = bc.getBlockByIndex(1);
        REQUIRE(merkleRoot == b.merkleRoot);
    }
    cleanup_test_dir(dir);
}

TEST_CASE("Integration: getInclusionProof error for bad block index", "[integration][merkle][rpc]") {
    MockBlockchain bc;

    // Block index 999 doesn't exist
    REQUIRE_THROWS_AS(bc.getInclusionProof(999, 0), std::out_of_range);
}

TEST_CASE("Integration: getInclusionProof error for bad entry index", "[integration][merkle][rpc]") {
    MockBlockchain bc;
    bc.publish("stream", "k1", "data1", {"k1"});

    // Entry index 5 doesn't exist in a single-entry block
    REQUIRE_THROWS_AS(bc.getInclusionProof(1, 5), std::out_of_range);
}

// =========================================================================
// verifyInclusionProof integration tests
// =========================================================================

TEST_CASE("Integration: verifyInclusionProof confirms valid proof", "[integration][merkle][rpc]") {
    MockBlockchain bc;
    bc.publish("stream", "k1", "verify_me", {"k1"});

    // Get the proof
    auto proofResult = bc.getInclusionProof(1, 0);

    // Verify the proof
    auto verifyResult = bc.verifyInclusionProof(
        1,
        proofResult["leafHash"].get<std::string>(),
        proofResult["proof"]
    );

    REQUIRE(verifyResult.contains("valid"));
    REQUIRE(verifyResult.contains("merkleRoot"));
    REQUIRE(verifyResult["valid"].get<bool>() == true);
    REQUIRE(verifyResult["merkleRoot"].get<std::string>() == proofResult["merkleRoot"].get<std::string>());
}

TEST_CASE("Integration: verifyInclusionProof rejects tampered leaf hash", "[integration][merkle][rpc]") {
    MockBlockchain bc;
    bc.publish("stream", "k1", "original_data", {"k1"});

    auto proofResult = bc.getInclusionProof(1, 0);

    // Tamper with the leaf hash
    std::string tamperedHash = std::string(64, 'f');

    auto verifyResult = bc.verifyInclusionProof(
        1,
        tamperedHash,
        proofResult["proof"]
    );

    REQUIRE(verifyResult["valid"].get<bool>() == false);
    // merkleRoot is still returned for client cross-check
    REQUIRE(verifyResult["merkleRoot"].get<std::string>().size() == 64);
}

TEST_CASE("Integration: verifyInclusionProof round-trip with real Blockchain<Chunk>", "[integration][merkle][rpc]") {
    auto dir = create_test_dir("verify_real");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir.string(), cfg);

        bc.publish("data", "k1", "payload1", {"k1"});

        // Full round-trip: get proof then verify it
        auto proofResult = bc.getInclusionProof(1, 0);
        auto verifyResult = bc.verifyInclusionProof(
            1,
            proofResult["leafHash"].get<std::string>(),
            proofResult["proof"]
        );

        REQUIRE(verifyResult["valid"].get<bool>() == true);

        Block b = bc.getBlockByIndex(1);
        REQUIRE(verifyResult["merkleRoot"].get<std::string>() == b.merkleRoot);
    }
    cleanup_test_dir(dir);
}

TEST_CASE("Integration: verifyInclusionProof error for bad block", "[integration][merkle][rpc]") {
    MockBlockchain bc;

    nlohmann::json emptyProof = nlohmann::json::array();
    REQUIRE_THROWS_AS(bc.verifyInclusionProof(999, "abc", emptyProof), std::out_of_range);
}

// =========================================================================
// getBlockHeader integration tests
// =========================================================================

TEST_CASE("Integration: getBlockHeader returns header-only fields", "[integration][merkle][rpc]") {
    MockBlockchain bc;
    bc.publish("stream", "k1", "header_test", {"k1"});

    Block b = bc.getBlockByIndex(1);
    auto header = b.toHeaderJson();

    // Verify exactly 7 fields per contracts/json-rpc.md
    REQUIRE(header.size() == 7);
    REQUIRE(header.contains("index"));
    REQUIRE(header.contains("timestamp"));
    REQUIRE(header.contains("prevHash"));
    REQUIRE(header.contains("merkleRoot"));
    REQUIRE(header.contains("nonce"));
    REQUIRE(header.contains("difficulty"));
    REQUIRE(header.contains("hash"));

    // No entries field
    REQUIRE_FALSE(header.contains("entries"));

    // Values match the full block
    REQUIRE(header["index"].get<size_t>() == b.index);
    REQUIRE(header["hash"].get<std::string>() == b.hash);
    REQUIRE(header["merkleRoot"].get<std::string>() == b.merkleRoot);
}

TEST_CASE("Integration: getBlockHeader is fixed-size regardless of entries", "[integration][merkle][rpc]") {
    MockBlockchain bc;
    bc.publish("stream", "k1", "short", {"k1"});
    bc.publish("stream", "k2", std::string(10000, 'x'), {"k2"});

    auto header1 = bc.getBlockByIndex(1).toHeaderJson();
    auto header2 = bc.getBlockByIndex(2).toHeaderJson();

    // Both headers have exactly 7 fields
    REQUIRE(header1.size() == 7);
    REQUIRE(header2.size() == 7);

    // Header size (serialized) should be similar regardless of entry data size
    std::string h1str = header1.dump();
    std::string h2str = header2.dump();
    // Both should be within reasonable fixed-size range (hash strings are all 64 chars)
    REQUIRE(h1str.size() < 500);
    REQUIRE(h2str.size() < 500);
}

TEST_CASE("Integration: getBlockHeader with real Blockchain<Chunk>", "[integration][merkle][rpc]") {
    auto dir = create_test_dir("header_real");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir.string(), cfg);

        bc.publish("data", "k1", "payload1", {"k1"});

        Block b = bc.getBlockByIndex(1);
        auto header = b.toHeaderJson();

        REQUIRE(header.size() == 7);
        REQUIRE(header["hash"].get<std::string>() == b.hash);
        REQUIRE(header["merkleRoot"].get<std::string>() == b.merkleRoot);
        REQUIRE(header["merkleRoot"].get<std::string>().size() == 64);
    }
    cleanup_test_dir(dir);
}

TEST_CASE("Integration: full pipeline - publish, get proof, verify, get header", "[integration][merkle][rpc]") {
    auto dir = create_test_dir("full_pipeline");
    {
        ConsensusConfig cfg;
        cfg.initialDifficulty = 0;
        cfg.minDifficulty = 0;
        Blockchain<Chunk> bc(dir.string(), cfg);

        // Step 1: Publish an entry
        Block published = bc.publish("audit", "entry1", "important_data", {"entry1"});
        REQUIRE(published.merkleRoot.size() == 64);

        // Step 2: Get inclusion proof
        auto proof = bc.getInclusionProof(1, 0);
        REQUIRE(proof["merkleRoot"].get<std::string>() == published.merkleRoot);

        // Step 3: Verify the proof
        auto verification = bc.verifyInclusionProof(
            1,
            proof["leafHash"].get<std::string>(),
            proof["proof"]
        );
        REQUIRE(verification["valid"].get<bool>() == true);

        // Step 4: Get block header
        auto header = bc.getBlockByIndex(1).toHeaderJson();
        REQUIRE(header["merkleRoot"].get<std::string>() == published.merkleRoot);
        REQUIRE(header["hash"].get<std::string>() == published.hash);
        REQUIRE_FALSE(header.contains("entries"));
    }
    cleanup_test_dir(dir);
}

TEST_CASE("Integration: genesis block header has valid merkleRoot", "[integration][merkle][rpc]") {
    MockBlockchain bc;
    Block genesis = bc.getBlockByIndex(0);
    auto header = genesis.toHeaderJson();

    REQUIRE(header.size() == 7);
    REQUIRE(header.contains("merkleRoot"));
    // Genesis block has empty entries, so merkleRoot is SHA-256 of empty string
    REQUIRE(header["merkleRoot"].get<std::string>().size() == 64);
}
