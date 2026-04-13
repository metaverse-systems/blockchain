#include "PeerManager.hpp"
#include "BlockPropagation.hpp"
#include "network/PeerClient.hpp"
#include "network/PeerServer.hpp"
#include "network/PeerMessages.hpp"
#include "network/PacketHeader.hpp"
#include "utils.hpp"
#include <fstream>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <random>

std::string PeerManager::peer_key(const std::string &host, uint16_t port) {
    return host + ":" + std::to_string(port);
}

PeerManager::PeerManager(boost::asio::io_context &io_context,
                         boost::asio::ssl::context &ssl_context,
                         const PeerConfig &config,
                         const std::filesystem::path &data_dir,
                         IBlockchain &bc,
                         SyncStatus &sync_status,
                         uint16_t p2p_port)
    : io_context_(io_context),
      ssl_context_(ssl_context),
      config_(config),
      data_dir_(data_dir),
      bc_(bc),
      sync_status_(sync_status),
      p2p_port_(p2p_port)
{
}

// --- Peer List Persistence ---

void PeerManager::load_peers() {
    auto peers_path = data_dir_ / "peers.json";
    if (!std::filesystem::exists(peers_path)) {
        node_uuid_ = generate_uuid_v4();
        LOG_INFO("Generated new node UUID: " + node_uuid_);
        save_peers();
        return;
    }

    std::ifstream ifs(peers_path);
    if (!ifs.is_open()) {
        node_uuid_ = generate_uuid_v4();
        LOG_WARN("Cannot open peers.json, starting fresh with UUID: " + node_uuid_);
        save_peers();
        return;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(ifs);
    } catch (const nlohmann::json::parse_error &e) {
        LOG_WARN("Malformed peers.json, starting with empty peer list: " + std::string(e.what()));
        node_uuid_ = generate_uuid_v4();
        save_peers();
        return;
    }

    if (j.contains("node_uuid") && j["node_uuid"].is_string()) {
        node_uuid_ = j["node_uuid"].get<std::string>();
    } else {
        node_uuid_ = generate_uuid_v4();
    }

    if (j.contains("peers") && j["peers"].is_array()) {
        auto peer_vec = j["peers"].get<std::vector<PeerEntry>>();
        for (auto &p : peer_vec) {
            peers_[peer_key(p.host, p.port)] = std::move(p);
        }
    }

    if (j.contains("bans") && j["bans"].is_array()) {
        auto ban_vec = j["bans"].get<std::vector<BanRecord>>();
        for (auto &b : ban_vec) {
            bans_[peer_key(b.host, b.port)] = std::move(b);
        }
    }

    LOG_INFO("Loaded " + std::to_string(peers_.size()) + " peers, UUID: " + node_uuid_);
}

void PeerManager::save_peers() {
    auto peers_path = data_dir_ / "peers.json";
    auto temp_path = data_dir_ / "peers.json.tmp";

    nlohmann::json j;
    j["node_uuid"] = node_uuid_;
    
    // Serialize map values as JSON arrays to preserve on-disk format
    nlohmann::json peers_arr = nlohmann::json::array();
    for (const auto &[key, entry] : peers_) {
        nlohmann::json pj;
        pj["host"] = entry.host;
        pj["port"] = entry.port;
        pj["node_uuid"] = entry.node_uuid;
        pj["last_seen"] = entry.last_seen;
        pj["error_count"] = entry.error_count;
        peers_arr.push_back(pj);
    }
    j["peers"] = peers_arr;

    nlohmann::json bans_arr = nlohmann::json::array();
    for (const auto &[key, ban] : bans_) {
        nlohmann::json bj;
        bj["host"] = ban.host;
        bj["port"] = ban.port;
        bj["reason"] = ban.reason;
        bj["expires"] = ban.expires;
        bans_arr.push_back(bj);
    }
    j["bans"] = bans_arr;

    std::ofstream ofs(temp_path);
    if (!ofs.is_open()) {
        LOG_ERROR("Cannot write peers.json.tmp");
        return;
    }
    ofs << j.dump(2) << std::endl;
    ofs.close();

    std::error_code ec;
    std::filesystem::rename(temp_path, peers_path, ec);
    if (ec) {
        LOG_ERROR("Failed to rename peers.json.tmp: " + ec.message());
    }
}

// --- Address Normalization ---

static std::string normalize_address(const std::string &host) {
    // Strip IPv4-mapped IPv6 prefix so "::ffff:127.0.0.1" becomes "127.0.0.1"
    const std::string prefix = "::ffff:";
    if (host.size() > prefix.size() &&
        host.compare(0, prefix.size(), prefix) == 0) {
        return host.substr(prefix.size());
    }
    return host;
}

// --- Peer List Management ---

bool PeerManager::add_peer(const PeerEntry &entry) {
    auto norm_host = normalize_address(entry.host);
    auto key = peer_key(norm_host, entry.port);

    auto it = peers_.find(key);
    if (it != peers_.end()) {
        // Update existing entry
        if (!entry.node_uuid.empty()) it->second.node_uuid = entry.node_uuid;
        if (entry.last_seen > it->second.last_seen) it->second.last_seen = entry.last_seen;
        return true;
    }

    // Cap enforcement with oldest-seen eviction
    if (peers_.size() >= static_cast<size_t>(config_.max_stored_peers)) {
        evict_oldest_peer();
    }

    PeerEntry new_entry = entry;
    new_entry.host = norm_host;
    peers_[key] = std::move(new_entry);
    return true;
}

void PeerManager::evict_oldest_peer() {
    if (peers_.empty()) return;

    auto oldest = peers_.begin();
    for (auto it = peers_.begin(); it != peers_.end(); ++it) {
        if (it->second.last_seen < oldest->second.last_seen) {
            oldest = it;
        }
    }
    peers_.erase(oldest);
}

bool PeerManager::remove_peer(const std::string &host_raw, uint16_t port) {
    auto host = normalize_address(host_raw);
    auto key = peer_key(host, port);
    return peers_.erase(key) > 0;
}

std::vector<PeerEntry> PeerManager::get_peers() const {
    std::vector<PeerEntry> result;
    result.reserve(peers_.size());
    for (const auto &[key, entry] : peers_) {
        result.push_back(entry);
    }
    return result;
}

PeerEntry* PeerManager::find_peer(const std::string &host_raw, uint16_t port) {
    auto host = normalize_address(host_raw);
    auto it = peers_.find(peer_key(host, port));
    if (it != peers_.end()) return &it->second;
    return nullptr;
}

const PeerEntry* PeerManager::find_peer(const std::string &host_raw, uint16_t port) const {
    auto host = normalize_address(host_raw);
    auto it = peers_.find(peer_key(host, port));
    if (it != peers_.end()) return &it->second;
    return nullptr;
}

// --- Self-Filtering ---

static bool is_loopback_address(const std::string &host) {
    auto norm = normalize_address(host);
    return norm == "127.0.0.1" || norm == "::1" || norm == "localhost";
}

bool PeerManager::is_self(const std::string &host, uint16_t port) const {
    if (port != p2p_port_) return false;
    return is_loopback_address(host);
}

void PeerManager::filter_self(std::vector<PeerAddress> &peers) const {
    peers.erase(
        std::remove_if(peers.begin(), peers.end(),
            [this](const PeerAddress &addr) {
                return is_self(addr.host, addr.port);
            }),
        peers.end()
    );
}

// --- Connection Management ---

void PeerManager::start() {
    load_peers();
    purge_expired_bans();

    if (config_.discovery_enabled) {
        // Connect to seed nodes
        for (const auto &seed : config_.seed_nodes) {
            if (outbound_connections_.size() >= config_.max_outbound) break;
            if (is_banned(seed.host, seed.port)) continue;
            connect_to(seed.host, seed.port);
        }

        // Also connect to known peers from peers.json
        for (const auto &[pk, peer] : peers_) {
            if (outbound_connections_.size() >= config_.max_outbound) break;
            if (is_banned(peer.host, peer.port)) continue;
            if (outbound_connections_.count(pk)) continue;
            connect_to(peer.host, peer.port);
        }

        start_exchange_timer();
    }
}

void PeerManager::connect_to(const std::string &host_raw, uint16_t port) {
    auto host = normalize_address(host_raw);
    auto key = peer_key(host, port);

    // Check limits
    if (outbound_connections_.size() >= config_.max_outbound) {
        LOG_WARN("Outbound connection limit reached, cannot connect to " + key);
        return;
    }

    // Check if already connected
    if (outbound_connections_.count(key)) {
        return;
    }

    // Check ban
    if (is_banned(host, port)) {
        LOG_WARN("Peer " + key + " is banned, skipping connection");
        return;
    }

    // Never connect to ourselves
    if (is_self(host, port)) {
        LOG_DEBUG("Skipping self-connection to " + key);
        return;
    }

    LOG_INFO("Connecting to peer " + key);

    auto client = std::make_shared<PeerClient>(io_context_, ssl_context_, host, port, bc_, sync_status_);
    client->set_peer_manager(this);
    if (block_propagation_) {
        client->set_block_propagation(block_propagation_);
    }
    outbound_connections_[key] = client;
    client->connect();
}

bool PeerManager::can_accept_inbound() const {
    return inbound_count_ < config_.max_inbound;
}

void PeerManager::on_peer_disconnected(const std::string &host_ref, uint16_t port) {
    // Copy and normalize host before erasing — the reference may belong to the PeerClient
    // that outbound_connections_.erase() is about to destroy.
    std::string host = normalize_address(host_ref);
    auto key = peer_key(host, port);
    outbound_connections_.erase(key);

    auto *peer = find_peer(host, port);
    if (peer) {
        peer->error_count++;
    }

    LOG_INFO("Peer disconnected: " + key);

    // Check if we still have an inbound session from the same node (e.g. dedup dropped
    // our outbound but the inbound is still alive). If so, skip reconnect.
    bool have_inbound = false;
    if (peer) {
        for (auto &[ik, weak_session] : inbound_sessions_) {
            auto session = weak_session.lock();
            if (session && session->get_remote_uuid() == peer->node_uuid) {
                have_inbound = true;
                break;
            }
        }
    }

    // Schedule reconnect if not banned and no inbound session exists
    if (!have_inbound && !is_banned(host, port)) {
        schedule_reconnect(host, port);
    }

    // Try to replace with a *different* known peer
    if (config_.discovery_enabled && outbound_connections_.size() < config_.max_outbound) {
        for (const auto &[pk, p] : peers_) {
            if (pk == key) continue; // Skip the peer that just disconnected
            if (outbound_connections_.count(pk)) continue;
            if (is_banned(p.host, p.port)) continue;
            if (backoff_state_.count(pk)) continue; // Already scheduled for reconnect
            connect_to(p.host, p.port);
            break;
        }
    }
}

void PeerManager::on_inbound_connected(const std::string &host_raw, uint16_t port, std::shared_ptr<PeerServer> session) {
    auto host = normalize_address(host_raw);
    auto key = peer_key(host, port);
    inbound_sessions_[key] = session;
    inbound_count_++;
    LOG_INFO("Inbound connection from " + key + " (total: " + std::to_string(inbound_count_) + ")");
}

void PeerManager::on_inbound_disconnected(const std::string &host_raw, uint16_t port) {
    auto host = normalize_address(host_raw);
    auto key = peer_key(host, port);
    inbound_sessions_.erase(key);
    if (inbound_count_ > 0) inbound_count_--;
    LOG_INFO("Inbound disconnection from " + key + " (total: " + std::to_string(inbound_count_) + ")");
}

// --- Peer Exchange ---

void PeerManager::on_peer_exchange_received(const std::string &sender_uuid, uint16_t sender_port,
                                             const std::string &sender_host_raw,
                                             const std::vector<PeerAddress> &received_peers) {
    auto sender_host = normalize_address(sender_host_raw);
    auto now = static_cast<uint64_t>(std::time(nullptr));

    // Update sender in our peer list
    auto *sender_peer = find_peer(sender_host, sender_port);
    if (sender_peer) {
        sender_peer->node_uuid = sender_uuid;
        sender_peer->last_seen = now;
        sender_peer->error_count = 0;
    } else {
        PeerEntry entry;
        entry.host = sender_host;
        entry.port = sender_port;
        entry.node_uuid = sender_uuid;
        entry.last_seen = now;
        add_peer(entry);
    }

    // Merge received peers
    std::vector<PeerAddress> new_peers;
    for (const auto &addr : received_peers) {
        auto norm_addr_host = normalize_address(addr.host);
        // Skip sender
        if (norm_addr_host == sender_host && addr.port == sender_port) continue;
        // Skip our own address
        if (is_self(norm_addr_host, addr.port)) continue;
        // Skip banned
        if (is_banned(norm_addr_host, addr.port)) continue;

        auto *existing = find_peer(norm_addr_host, addr.port);
        if (existing) {
            // Update last_seen
            existing->last_seen = now;
        } else {
            PeerEntry entry;
            entry.host = norm_addr_host;
            entry.port = addr.port;
            entry.last_seen = now;
            add_peer(entry);
            new_peers.push_back(addr);
        }
    }

    save_peers();

    // Connect to newly discovered peers
    if (config_.discovery_enabled && !new_peers.empty()) {
        on_new_peers_discovered(new_peers);
    }
}

void PeerManager::broadcast_peer_exchange() {
    auto addresses = get_non_banned_peer_addresses();

    for (auto &[key, client] : outbound_connections_) {
        if (client && client->is_connected()) {
            PeerExchangeRequest req;
            req.sender_uuid = node_uuid_;
            req.sender_listen_port = p2p_port_;
            req.peers = addresses;
            client->send(req, PacketType::PEER_EXCHANGE);
        }
    }
}

void PeerManager::on_new_peers_discovered(const std::vector<PeerAddress> &new_peers) {
    for (const auto &addr : new_peers) {
        if (outbound_connections_.size() >= config_.max_outbound) break;
        auto key = peer_key(addr.host, addr.port);
        if (outbound_connections_.count(key)) continue;
        if (is_banned(addr.host, addr.port)) continue;
        connect_to(addr.host, addr.port);
    }
}

// --- Duplicate Connection Detection ---

void PeerManager::check_duplicate_connection(const std::string &remote_uuid,
                                              const std::string &host, uint16_t port) {
    if (remote_uuid.empty()) return;

    // If the remote UUID is our own, it's a self-connection — drop immediately
    if (remote_uuid == node_uuid_) {
        LOG_WARN("Self-connection detected (UUID " + remote_uuid + ") — closing");
        auto self_key = peer_key(host, port);
        // Defer the erase so the calling PeerClient is not destroyed mid-call
        boost::asio::post(io_context_, [this, self_key]() {
            outbound_connections_.erase(self_key);
        });
        return;
    }

    // Look for an existing outbound connection with the same remote UUID
    auto current_key = peer_key(host, port);
    std::string dup_outbound_key;
    for (auto &[key, client] : outbound_connections_) {
        if (!client) continue;
        if (client->get_remote_uuid() == remote_uuid) {
            dup_outbound_key = key;
            break;
        }
    }

    // Also check if we have an inbound session from this UUID
    bool has_inbound = false;
    for (auto &[key, weak_session] : inbound_sessions_) {
        auto session = weak_session.lock();
        if (session && session->get_remote_uuid() == remote_uuid) {
            has_inbound = true;
            break;
        }
    }

    // Duplicate means we have both an outbound connection and an inbound session
    // for the same remote UUID
    if (dup_outbound_key.empty() || !has_inbound) return;

    // The dedup rule: lower UUID keeps its outbound
    if (node_uuid_ < remote_uuid) {
        LOG_INFO("Duplicate connection detected for UUID " + remote_uuid + " — keeping our outbound (lower UUID)");
        // The remote (higher UUID) should drop its outbound to us; nothing to do here
    } else {
        LOG_INFO("Duplicate connection detected for UUID " + remote_uuid + " — dropping our outbound (higher UUID)");
        // Drop our outbound connection; keep the inbound
        boost::asio::post(io_context_, [this, dup_outbound_key]() {
            outbound_connections_.erase(dup_outbound_key);
        });
    }
}

// --- Ban Management ---

void PeerManager::increment_error(const std::string &host, uint16_t port) {
    auto *peer = find_peer(host, port);
    if (peer) {
        peer->error_count++;
        if (peer->error_count >= config_.ban_threshold_errors) {
            ban_peer(host, port, "excessive_errors", config_.ban_duration_seconds);
        }
    }
}

void PeerManager::ban_peer(const std::string &host, uint16_t port, const std::string &reason, uint64_t duration_seconds) {
    // Remove from active outbound connections
    auto key = peer_key(host, port);
    auto it = outbound_connections_.find(key);
    if (it != outbound_connections_.end()) {
        // Close the connection
        outbound_connections_.erase(it);
    }

    // Remove from peer list
    remove_peer(host, port);

    // Add ban record
    BanRecord ban;
    ban.host = host;
    ban.port = port;
    ban.reason = reason;
    if (duration_seconds > 0) {
        ban.expires = static_cast<uint64_t>(std::time(nullptr)) + duration_seconds;
    } else {
        ban.expires = 0; // Permanent until unban
    }

    // Remove existing ban for same address
    unban_peer(host, port);
    bans_[peer_key(host, port)] = ban;

    save_peers();
    LOG_INFO("Banned peer " + key + " reason: " + reason);
}

void PeerManager::unban_peer(const std::string &host, uint16_t port) {
    bans_.erase(peer_key(host, port));
}

bool PeerManager::is_banned(const std::string &host, uint16_t port) const {
    auto it = bans_.find(peer_key(host, port));
    if (it == bans_.end()) return false;
    auto now = static_cast<uint64_t>(std::time(nullptr));
    return it->second.expires == 0 || it->second.expires > now;
}

void PeerManager::purge_expired_bans() {
    auto now = static_cast<uint64_t>(std::time(nullptr));
    for (auto it = bans_.begin(); it != bans_.end(); ) {
        if (it->second.expires > 0 && it->second.expires <= now) {
            it = bans_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<BanRecord> PeerManager::get_bans() const {
    std::vector<BanRecord> result;
    result.reserve(bans_.size());
    for (const auto &[key, ban] : bans_) {
        result.push_back(ban);
    }
    return result;
}

// --- Reconnection with Backoff ---

void PeerManager::schedule_reconnect(const std::string &host, uint16_t port) {
    auto key = peer_key(host, port);

    auto &state = backoff_state_[key];
    if (state.current_delay == 0) {
        state.current_delay = config_.reconnect_base_delay_seconds;
    } else {
        state.current_delay = std::min(state.current_delay * 2, config_.reconnect_max_delay_seconds);
    }

    // Add ±20% jitter
    std::random_device rd;
    std::mt19937 gen(rd());
    double jitter_factor = 0.8 + (std::uniform_real_distribution<double>(0.0, 0.4)(gen));
    auto delay_ms = static_cast<uint64_t>(state.current_delay * jitter_factor * 1000);

    LOG_INFO("Scheduling reconnect to " + key + " in " + std::to_string(delay_ms / 1000) + "s");

    state.timer = std::make_shared<boost::asio::steady_timer>(io_context_);
    state.timer->expires_after(std::chrono::milliseconds(delay_ms));
    state.timer->async_wait([this, host, port, key](const boost::system::error_code &ec) {
        if (!ec) {
            backoff_state_.erase(key);
            if (!is_banned(host, port) && outbound_connections_.size() < config_.max_outbound) {
                connect_to(host, port);
            }
        }
    });
}

void PeerManager::reset_backoff(const std::string &host_raw, uint16_t port) {
    backoff_state_.erase(peer_key(normalize_address(host_raw), port));
}

// --- Exchange Timer ---

void PeerManager::start_exchange_timer() {
    exchange_timer_ = std::make_shared<boost::asio::steady_timer>(io_context_);
    exchange_timer_->expires_after(std::chrono::seconds(config_.exchange_interval_seconds));
    exchange_timer_->async_wait([this](const boost::system::error_code &ec) {
        if (!ec) {
            broadcast_peer_exchange();
            start_exchange_timer(); // Reschedule
        }
    });
}

// --- Accessors ---

uint16_t PeerManager::get_listen_port() const {
    return p2p_port_;
}

std::vector<PeerAddress> PeerManager::get_non_banned_peer_addresses() const {
    std::vector<PeerAddress> addresses;
    for (const auto &[key, p] : peers_) {
        if (!is_banned(p.host, p.port)) {
            addresses.push_back({p.host, p.port});
        }
    }
    return addresses;
}

size_t PeerManager::outbound_count() const {
    return outbound_connections_.size();
}

size_t PeerManager::inbound_count() const {
    return inbound_count_;
}

void PeerManager::disconnect_and_remove(const std::string &host_raw, uint16_t port) {
    auto host = normalize_address(host_raw);
    auto key = peer_key(host, port);
    outbound_connections_.erase(key);
    remove_peer(host, port);
    save_peers();
}

// --- Block Propagation ---

void PeerManager::send_to_peers(const Block &block, const std::string &exclude_key) {
    // Send to all outbound connections except the sender
    for (auto &[key, client] : outbound_connections_) {
        if (!exclude_key.empty() && key == exclude_key) continue;
        if (client && client->is_connected()) {
            client->send(block, PacketType::BLOCK);
        }
    }

    // Send to all tracked inbound sessions except the sender
    for (auto it = inbound_sessions_.begin(); it != inbound_sessions_.end(); ) {
        auto session = it->second.lock();
        if (session) {
            if (exclude_key.empty() || it->first != exclude_key) {
                session->send_packet_public(block, PacketType::BLOCK);
            }
            ++it;
        } else {
            it = inbound_sessions_.erase(it);
        }
    }
}

void PeerManager::broadcast_block(const Block &block) {
    send_to_peers(block);
}

void PeerManager::relay_block(const Block &block, const std::string &exclude_key) {
    send_to_peers(block, exclude_key);
}
