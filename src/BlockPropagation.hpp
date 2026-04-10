#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <deque>
#include <functional>
#include <chrono>
#include "Block.hpp"
#include "IBlockchain.hpp"
#include "SyncState.hpp"

class PeerManager;

struct PendingBlock {
    Block block;
    std::string sender_key;
    std::chrono::steady_clock::time_point inserted_at;
};

struct BlockRateState {
    uint32_t count = 0;
    std::chrono::steady_clock::time_point window_start;
};

class BlockPropagation {
public:
    using RelayCallback = std::function<void(const Block&, const std::string&)>;

    BlockPropagation(IBlockchain &bc, SyncStatus &sync_status, RelayCallback relay_cb);

    void on_block_received(const Block &block, const std::string &sender_key);
    void process_sync_queue();

    void set_peer_manager(PeerManager *pm) { peer_manager_ = pm; }

private:
    IBlockchain &bc_;
    SyncStatus &sync_status_;
    RelayCallback relay_cb_;
    PeerManager *peer_manager_ = nullptr;

    // RecentBlockCache (max 512)
    std::unordered_set<std::string> dedup_set_;
    std::deque<std::string> dedup_order_;
    static constexpr size_t kMaxDedupCache = 512;

    // PendingBlock pool (max 64, 60s TTL)
    std::unordered_map<std::string, PendingBlock> pending_pool_;
    static constexpr size_t kMaxPendingPool = 64;
    static constexpr std::chrono::seconds kPendingTTL{60};

    // SyncBlockQueue (max 128)
    std::deque<std::pair<Block, std::string>> sync_queue_;
    static constexpr size_t kMaxSyncQueue = 128;

    // Per-peer rate limiting
    std::unordered_map<std::string, BlockRateState> rate_states_;
    static constexpr uint32_t kRateLimitPerSecond = 10;

    // RecentBlockCache helpers
    bool cache_contains(const std::string &hash) const;
    void cache_insert(const std::string &hash);

    // Rate limiting
    bool check_rate_limit(const std::string &sender_key);

    // Pending pool
    void defer_block(const Block &block, const std::string &sender_key);
    void resolve_pending(const std::string &new_block_hash);
    void evict_expired();

    // Append helper
    void appendReceivedBlock(const Block &block);
};
