#pragma once

#include "Block.hpp"
#include "StreamEntry.hpp"
#include "ConsensusConfig.hpp"
#include <set>
#include <string>
#include <vector>
#include <utility>

class IChainReader {
public:
    virtual ~IChainReader() = default;
    virtual bool isShuttingDown() const = 0;
    virtual std::set<std::string> listStreams() const = 0;
    virtual std::vector<std::pair<size_t, StreamEntry>> getStreamEntries(
        const std::string &stream, const std::string &key = "") const = 0;
    virtual std::pair<size_t, StreamEntry> getStreamEntry(
        const std::string &stream, const std::string &key) const = 0;
    virtual size_t getChainBlockCount() const = 0;
    virtual size_t getChainLength() const = 0;
    virtual size_t getChunkCount() const = 0;
    virtual uint32_t getCurrentDifficulty() const = 0;
    virtual const ConsensusConfig& getConfig() const = 0;
};
