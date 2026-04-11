#include "BlockPropagation.hpp"
#include "PeerManager.hpp"
#include "utils.hpp"
#include "ConsensusConfig.hpp"

BlockPropagation::BlockPropagation(IBlockchain &bc, SyncStatus &sync_status, RelayCallback relay_cb)
    : bc_(bc), sync_status_(sync_status), relay_cb_(std::move(relay_cb))
{
}

// --- RecentBlockCache ---

bool BlockPropagation::cache_contains(const std::string &hash) const
{
    return dedup_set_.count(hash) > 0;
}

void BlockPropagation::cache_insert(const std::string &hash)
{
    if (dedup_set_.count(hash)) return;

    if (dedup_set_.size() >= kMaxDedupCache) {
        // FIFO eviction
        const auto &oldest = dedup_order_.front();
        dedup_set_.erase(oldest);
        dedup_order_.pop_front();
    }

    dedup_set_.insert(hash);
    dedup_order_.push_back(hash);
}

// --- Rate Limiting ---

bool BlockPropagation::check_rate_limit(const std::string &sender_key)
{
    auto now = std::chrono::steady_clock::now();
    auto &state = rate_states_[sender_key];

    if (now - state.window_start > std::chrono::seconds(1)) {
        state.count = 0;
        state.window_start = now;
    }

    if (state.count >= kRateLimitPerSecond) {
        return false;
    }

    state.count++;
    return true;
}

// --- Pending Pool ---

void BlockPropagation::defer_block(const Block &block, const std::string &sender_key)
{
    evict_expired();

    if (pending_pool_.size() >= kMaxPendingPool) {
        // Evict oldest entry
        auto oldest_it = pending_pool_.begin();
        auto oldest_time = oldest_it->second.inserted_at;
        for (auto it = pending_pool_.begin(); it != pending_pool_.end(); ++it) {
            if (it->second.inserted_at < oldest_time) {
                oldest_it = it;
                oldest_time = it->second.inserted_at;
            }
        }
        pending_pool_.erase(oldest_it);
    }

    PendingBlock pb;
    pb.block = block;
    pb.sender_key = sender_key;
    pb.inserted_at = std::chrono::steady_clock::now();
    pending_pool_[block.prevHash] = std::move(pb);

    logMessage("INFO", "Deferred block #" + std::to_string(block.index) + " waiting for predecessor");
}

void BlockPropagation::resolve_pending(const std::string &new_block_hash)
{
    auto it = pending_pool_.find(new_block_hash);
    if (it != pending_pool_.end()) {
        auto pb = std::move(it->second);
        pending_pool_.erase(it);
        logMessage("INFO", "Resolving pending block #" + std::to_string(pb.block.index));
        on_block_received(pb.block, pb.sender_key);
    }
}

void BlockPropagation::evict_expired()
{
    auto now = std::chrono::steady_clock::now();
    for (auto it = pending_pool_.begin(); it != pending_pool_.end(); ) {
        if (now - it->second.inserted_at > kPendingTTL) {
            logMessage("INFO", "Evicting expired pending block #" + std::to_string(it->second.block.index));
            it = pending_pool_.erase(it);
        } else {
            ++it;
        }
    }
}

// --- Append Helper ---

void BlockPropagation::appendReceivedBlock(const Block &block)
{
    bc_.appendBlock(block);
    bc_.saveChunk(block.index / bc_.chunkSize);
    bc_.saveKeys();
}

// --- Core Reception ---

void BlockPropagation::on_block_received(const Block &block, const std::string &sender_key)
{
    // Rate limit check
    if (!check_rate_limit(sender_key)) {
        logMessage("WARN", "Rate limit exceeded for peer " + sender_key + ", dropping block #" + std::to_string(block.index));
        if (peer_manager_) {
            auto colon = sender_key.find(':');
            if (colon != std::string::npos) {
                auto host = sender_key.substr(0, colon);
                auto port = static_cast<uint16_t>(std::stoi(sender_key.substr(colon + 1)));
                peer_manager_->increment_error(host, port);
            }
        }
        return;
    }

    // Dedup cache check
    if (cache_contains(block.hash)) {
        return;
    }

    // Chain-tip index check: already in chain
    if (block.index < bc_.getChainBlockCount()) {
        return;
    }

    // Sync queue check
    if (sync_status_.isSyncing.load()) {
        if (sync_queue_.size() >= kMaxSyncQueue) {
            sync_queue_.pop_front();
        }
        sync_queue_.emplace_back(block, sender_key);
        logMessage("INFO", "Queued block #" + std::to_string(block.index) + " during sync");
        return;
    }

    // Check if block connects to chain tip
    size_t chain_height = bc_.getChainBlockCount();
    if (chain_height > 0) {
        Block tip = bc_.getBlockByIndex(chain_height - 1);

        if (block.prevHash != tip.hash) {
            // Gap block — defer
            defer_block(block, sender_key);
            return;
        }

        // Validate against consensus
        const auto &config = bc_.getConfig();
        if (!IBlockchain::isValidNewBlock(block, tip, config)) {
            logMessage("WARN", "Invalid block #" + std::to_string(block.index) + " from " + sender_key);
            if (peer_manager_) {
                auto colon = sender_key.find(':');
                if (colon != std::string::npos) {
                    auto host = sender_key.substr(0, colon);
                    auto port = static_cast<uint16_t>(std::stoi(sender_key.substr(colon + 1)));
                    peer_manager_->increment_error(host, port);
                }
            }
            return;
        }
    }

    // Append valid block
    appendReceivedBlock(block);
    logMessage("INFO", "Block #" + std::to_string(block.index) + " validated and appended");

    // Add to dedup cache
    cache_insert(block.hash);

    // Relay to other peers (excluding sender)
    if (relay_cb_) {
        relay_cb_(block, sender_key);
    }

    // Check pending pool for children
    resolve_pending(block.hash);
}

// --- Sync Queue Processing ---

void BlockPropagation::process_sync_queue()
{
    logMessage("INFO", "Processing sync queue (" + std::to_string(sync_queue_.size()) + " blocks)");

    auto queue = std::move(sync_queue_);
    sync_queue_.clear();

    for (auto &[block, sender_key] : queue) {
        if (cache_contains(block.hash)) continue;
        if (block.index < bc_.getChainBlockCount()) continue;
        on_block_received(block, sender_key);
    }
}
