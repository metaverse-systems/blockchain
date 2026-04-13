#include "BlockPropagation.hpp"
#include "PeerManager.hpp"
#include "utils.hpp"
#include "ConsensusConfig.hpp"
#include <algorithm>

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

    if (pending_map_.size() >= kMaxPendingPool) {
        // O(1) eviction: pop oldest from front of deque
        const auto &oldest_key = pending_order_.front();
        pending_map_.erase(oldest_key);
        pending_order_.pop_front();
    }

    const auto &key = block.prevHash;
    if (pending_map_.contains(key)) {
        // Already waiting on this prevHash — update entry, preserve order
        PendingBlock pb;
        pb.block = block;
        pb.sender_key = sender_key;
        pb.inserted_at = std::chrono::steady_clock::now();
        pending_map_[key] = std::move(pb);
    } else {
        PendingBlock pb;
        pb.block = block;
        pb.sender_key = sender_key;
        pb.inserted_at = std::chrono::steady_clock::now();
        pending_map_[key] = std::move(pb);
        pending_order_.push_back(key);
    }

    LOG_INFO("Deferred block #" + std::to_string(block.index) + " waiting for predecessor");
}

void BlockPropagation::resolve_pending(const std::string &new_block_hash)
{
    auto it = pending_map_.find(new_block_hash);
    if (it != pending_map_.end()) {
        auto pb = std::move(it->second);
        pending_map_.erase(it);
        // Remove from order deque
        auto order_it = std::find(pending_order_.begin(), pending_order_.end(), new_block_hash);
        if (order_it != pending_order_.end()) {
            pending_order_.erase(order_it);
        }
        LOG_INFO("Resolving pending block #" + std::to_string(pb.block.index));
        on_block_received(pb.block, pb.sender_key);
    }
}

void BlockPropagation::evict_expired()
{
    auto now = std::chrono::steady_clock::now();
    // Walk from front (oldest) and evict expired entries
    while (!pending_order_.empty()) {
        auto it = pending_map_.find(pending_order_.front());
        if (it == pending_map_.end()) {
            // Stale entry in order deque
            pending_order_.pop_front();
            continue;
        }
        if (now - it->second.inserted_at > kPendingTTL) {
            LOG_INFO("Evicting expired pending block #" + std::to_string(it->second.block.index));
            pending_map_.erase(it);
            pending_order_.pop_front();
        } else {
            break;
        }
    }
}

// --- Append Helper ---

void BlockPropagation::appendReceivedBlock(const Block &block)
{
    // Verify merkle root and hash integrity using verify-then-cache constructor
    try {
        Block verified(block.index, block.timestamp, block.prevHash,
                       block.entries, block.nonce, block.difficulty,
                       block.merkleRoot, block.hash);
        bc_.appendBlock(verified);
    } catch (const std::invalid_argument &e) {
        LOG_WARN("Block #" + std::to_string(block.index)
                   + " rejected: " + std::string(e.what()));
        return;
    }
    bc_.saveChunk(block.index / bc_.chunkSize);
    bc_.saveKeys();
}

// --- Core Reception ---

void BlockPropagation::on_block_received(const Block &block, const std::string &sender_key)
{
    // Rate limit check
    if (!check_rate_limit(sender_key)) {
        LOG_WARN("Rate limit exceeded for peer " + sender_key + ", dropping block #" + std::to_string(block.index));
        if (peer_manager_) {
            try {
                auto [host, port] = parsePeerKey(sender_key);
                peer_manager_->increment_error(host, port);
            } catch (const std::invalid_argument &) {}
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
        LOG_INFO("Queued block #" + std::to_string(block.index) + " during sync");
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
            LOG_WARN("Invalid block #" + std::to_string(block.index) + " from " + sender_key);
            if (peer_manager_) {
                try {
                    auto [host, port] = parsePeerKey(sender_key);
                    peer_manager_->increment_error(host, port);
                } catch (const std::invalid_argument &) {}
            }
            return;
        }
    }

    // Append valid block
    appendReceivedBlock(block);
    LOG_INFO("Block #" + std::to_string(block.index) + " validated and appended");

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
    LOG_INFO("Processing sync queue (" + std::to_string(sync_queue_.size()) + " blocks)");

    auto queue = std::move(sync_queue_);
    sync_queue_.clear();

    for (auto &[block, sender_key] : queue) {
        if (cache_contains(block.hash)) continue;
        if (block.index < bc_.getChainBlockCount()) continue;
        on_block_received(block, sender_key);
    }
}
