#pragma once

#include <string>
#include "Block.hpp"
#include "json.hpp"

class MerkleProofService
{
  public:
    MerkleProofService() = default;

    nlohmann::json getInclusionProof(const Block& block, size_t entryIndex);
    nlohmann::json verifyInclusionProof(const Block& block,
                                         const std::string& leafHash,
                                         const nlohmann::json& proofArray);
};
