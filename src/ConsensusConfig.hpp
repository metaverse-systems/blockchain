#pragma once
#include <cstdint>
#include <cstdlib>
#include <string>

struct ConsensusConfig {
    uint32_t targetBlockInterval = 10;
    uint32_t adjustmentWindow = 10;
    double maxAdjustmentFactor = 4.0;
    uint32_t minDifficulty = 1;
    uint32_t maxDifficulty = 16;
    uint32_t initialDifficulty = 1;
    uint32_t miningTimeout = 30;
    uint32_t maxFutureTimestamp = 120;
    uint32_t maxReorgDepth = 100;

    static ConsensusConfig fromEnv()
    {
        ConsensusConfig cfg;
        auto readUint = [](const char *name, uint32_t fallback) -> uint32_t {
            const char *val = std::getenv(name);
            if (!val) return fallback;
            try { return static_cast<uint32_t>(std::stoul(val)); }
            catch (...) { return fallback; }
        };
        auto readDouble = [](const char *name, double fallback) -> double {
            const char *val = std::getenv(name);
            if (!val) return fallback;
            try { return std::stod(val); }
            catch (...) { return fallback; }
        };

        cfg.targetBlockInterval = readUint("BLOCKCHAIN_TARGET_INTERVAL", cfg.targetBlockInterval);
        cfg.adjustmentWindow = readUint("BLOCKCHAIN_ADJUST_WINDOW", cfg.adjustmentWindow);
        cfg.maxAdjustmentFactor = readDouble("BLOCKCHAIN_MAX_ADJUST_FACTOR", cfg.maxAdjustmentFactor);
        cfg.minDifficulty = readUint("BLOCKCHAIN_MIN_DIFFICULTY", cfg.minDifficulty);
        cfg.maxDifficulty = readUint("BLOCKCHAIN_MAX_DIFFICULTY", cfg.maxDifficulty);
        cfg.initialDifficulty = readUint("BLOCKCHAIN_INITIAL_DIFFICULTY", cfg.initialDifficulty);
        cfg.miningTimeout = readUint("BLOCKCHAIN_MINING_TIMEOUT", cfg.miningTimeout);
        cfg.maxFutureTimestamp = readUint("BLOCKCHAIN_MAX_FUTURE_TIMESTAMP", cfg.maxFutureTimestamp);
        cfg.maxReorgDepth = readUint("BLOCKCHAIN_MAX_REORG_DEPTH", cfg.maxReorgDepth);
        return cfg;
    }
};
