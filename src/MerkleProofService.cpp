#include "MerkleProofService.hpp"
#include "MerkleTree.hpp"
#include "StreamEntry.hpp"
#include <sstream>
#include <stdexcept>
#include <boost/archive/binary_oarchive.hpp>

nlohmann::json MerkleProofService::getInclusionProof(const Block& block, size_t entryIndex)
{
    if (entryIndex >= block.entries.size()) {
        throw std::out_of_range("Entry index out of range: " + std::to_string(entryIndex));
    }

    std::vector<std::string> leafHashes;
    for (const auto &entry : block.entries) {
        std::ostringstream oss;
        boost::archive::binary_oarchive oa(oss);
        oa << entry;
        leafHashes.push_back(MerkleTree::computeLeafHash(oss.str()));
    }

    auto proof = MerkleTree::generateProof(leafHashes, entryIndex);

    nlohmann::json result;
    result["blockIndex"] = block.index;
    result["entryIndex"] = entryIndex;
    result["merkleRoot"] = block.merkleRoot;
    result["leafHash"] = leafHashes[entryIndex];

    nlohmann::json proofArray = nlohmann::json::array();
    for (const auto &elem : proof) {
        nlohmann::json pj;
        pj["hash"] = elem.hash;
        pj["isLeft"] = elem.isLeft;
        proofArray.push_back(pj);
    }
    result["proof"] = proofArray;

    return result;
}

nlohmann::json MerkleProofService::verifyInclusionProof(const Block& block,
                                                          const std::string& leafHash,
                                                          const nlohmann::json& proofArray)
{
    std::vector<MerkleProofElement> proof;
    for (const auto &elem : proofArray) {
        MerkleProofElement pe;
        pe.hash = elem["hash"].get<std::string>();
        pe.isLeft = elem["isLeft"].get<bool>();
        proof.push_back(pe);
    }

    bool valid = MerkleTree::verifyProof(block.merkleRoot, leafHash, proof);

    nlohmann::json result;
    result["valid"] = valid;
    result["merkleRoot"] = block.merkleRoot;
    return result;
}
