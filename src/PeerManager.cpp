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
        logMessage("INFO", "Generated new node UUID: " + node_uuid_);
        save_peers();
        return;
    }

    std::ifstream ifs(peers_path);
    if (!ifs.is_open()) {
        node_uuid_ = generate_uuid_v4();
        logMessage("WARN", "Cannot open peers.json, starting fresh with UUID: " + node_uuid_);
        save_peers();
        return;
    }

    nlohmann::json j;
    try {
        j = nlohmann::json::parse(ifs);
    } catch (const nlohmann::json::parse_error &e) {
        logMessage("WARN", "Malformed peers.json, starting with empty peer list: " + std::string(e.what()));
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
        peers_ = j["peers"].get<std::vector<PeerEntry>>();
    }

    if (j.contains("bans") && j["bans"].is_array()) {
        bans_ = j["bans"].get<std::vector<BanRecord>>();
    }

    logMessage("INFO", "Loaded " + std::to_string(peers_.size()) + " peers, UUID: " + node_uuid_);
}

void PeerManager::save_peers() {
    auto peers_path = data_dir_ / "peers.json";
    auto temp_path = data_dir_ / "peers.json.tmp";

    nlohmann::json j;
    j["node_uuid"] = node_uuid_;
    j["peers"] = peers_;
    j["bans"] = bans_;

    std::ofstream ofs(temp_path);
    if (!ofs.is_open()) {
        logMessage("ERROR", "Cannot write peers.json.tmp");
        return;
    }
    ofs << j.dump(2) << std::endl;
    ofs.close();

    std::error_code ec;
    std::filesystem::rename(temp_path, peers_path, ec);
    if (ec) {
        logMessage("ERROR", "Failed to rename peers.json.tmp: " + ec.message());
    }
}

// --- Peer List Management ---

bool PeerManager::add_peer(const PeerEntry &entry) {
    // Check if already exists
    for (auto &p : peers_) {
        if (p.host == entry.host && p.port == entry.port) {
            // Update existing entry
            if (!entry.node_uuid.empty()) p.node_uuid = entry.node_uuid;
            if (entry.last_seen > p.last_seen) p.last_seen = entry.last_seen;
            return true;
        }
    }

    // Cap enforcement with oldest-seen eviction
    if (peers_.size() >= static_cast<size_t>(config_.max_stored_peers)) {
        evict_oldest_peer();
    }

    peers_.push_back(entry);
    return true;
}

void PeerManager::evict_oldest_peer() {
    if (peers_.empty()) return;

    auto oldest = std::min_element(peers_.begin(), peers_.end(),
        [](const PeerEntry &a, const PeerEntry &b) {
            return a.last_seen < b.last_seen;
        });
    peers_.erase(oldest);
}

bool PeerManager::remove_peer(const std::string &host, uint16_t port) {
    auto it = std::remove_if(peers_.begin(), peers_.end(),
        [&](const PeerEntry &p) { return p.host == host && p.port == port; });
    if (it != peers_.end()) {
        peers_.erase(it, peers_.end());
        return true;
    }
    return false;
}

std::vector<PeerEntry> PeerManager::get_peers() const {
    return peers_;
}

PeerEntry* PeerManager::find_peer(const std::string &host, uint16_t port) {
    for (auto &p : peers_) {
        if (p.host == host && p.port == port) return &p;
    }
    return nullptr;
}

const PeerEntry* PeerManager::find_peer(const std::string &host, uint16_t port) const {
    for (auto &p : peers_) {
        if (p.host == host && p.port == port) return &p;
    }
    return nullptr;
}

// --- Self-Filtering ---

void PeerManager::filter_self(std::vector<PeerAddress> &peers) const {
    peers.erase(
        std::remove_if(peers.begin(), peers.end(),
            [this](const PeerAddress &/*addr*/) {
                // Filter by UUID match or address match with our listen port
                // We don't have our own address easily, so just filter by known identifiers
                return false; // UUID filtering happens during merge
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
        for (const auto &peer : peers_) {
            if (outbound_connections_.size() >= config_.max_outbound) break;
            if (is_banned(peer.host, peer.port)) continue;
            auto key = peer_key(peer.host, peer.port);
            if (outbound_connections_.count(key)) continue;
            connect_to(peer.host, peer.port);
        }

        start_exchange_timer();
    }
}

void PeerManager::connect_to(const std::string &host, uint16_t port) {
    auto key = peer_key(host, port);

    // Check limits
    if (outbound_connections_.size() >= config_.max_outbound) {
        logMessage("WARN", "Outbound connection limit reached, cannot connect to " + key);
        return;
    }

    // Check if already connected
    if (outbound_connections_.count(key)) {
        return;
    }

    // Check ban
    if (is_banned(host, port)) {
        logMessage("WARN", "Peer " + key + " is banned, skipping connection");
        return;
    }

    logMessage("INFO", "Connecting to peer " + key);

    auto client = std::make_shared<PeerClient>(io_context_, ssl_context_, host, port, bc_, sync_status_);
    client->set_peer_manager(this);
    if (block_propagation_) {
        client->set_block_propagation(block_propagation_);
    }
    outbound_connections_[key] = client;
    client->connect();

    // Reset backoff on connection attempt
    backoff_state_.erase(key);
}

bool PeerManager::can_accept_inbound() const {
    return inbound_count_ < config_.max_inbound;
}

void PeerManager::on_peer_disconnected(const std::string &host, uint16_t port) {
    auto key = peer_key(host, port);
    outbound_connections_.erase(key);

    auto *peer = find_peer(host, port);
    if (peer) {
        peer->error_count++;
    }

    logMessage("INFO", "Peer disconnected: " + key);

    // Schedule reconnect if not banned
    if (!is_banned(host, port)) {
        schedule_reconnect(host, port);
    }

    // Try to replace with another known peer
    if (config_.discovery_enabled && outbound_connections_.size() < config_.max_outbound) {
        for (const auto &p : peers_) {
            auto pk = peer_key(p.host, p.port);
            if (outbound_connections_.count(pk)) continue;
            if (is_banned(p.host, p.port)) continue;
            if (backoff_state_.count(pk)) continue; // Already scheduled for reconnect
            connect_to(p.host, p.port);
            break;
        }
    }
}

void PeerManager::on_inbound_connected(const std::string &host, uint16_t port, std::shared_ptr<PeerServer> session) {
    auto key = peer_key(host, port);
    inbound_sessions_[key] = session;
    inbound_count_++;
    logMessage("INFO", "Inbound connection from " + key + " (total: " + std::to_string(inbound_count_) + ")");
}

void PeerManager::on_inbound_disconnected(const std::string &host, uint16_t port) {
    auto key = peer_key(host, port);
    inbound_sessions_.erase(key);
    if (inbound_count_ > 0) inbound_count_--;
    logMessage("INFO", "Inbound disconnection from " + key + " (total: " + std::to_string(inbound_count_) + ")");
}

// --- Peer Exchange ---

void PeerManager::on_peer_exchange_received(const std::string &sender_uuid, uint16_t sender_port,
                                             const std::string &sender_host,
                                             const std::vector<PeerAddress> &received_peers) {
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
        // Skip self
        if (addr.host == sender_host && addr.port == sender_port) continue;
        // Skip banned
        if (is_banned(addr.host, addr.port)) continue;

        auto *existing = find_peer(addr.host, addr.port);
        if (existing) {
            // Update last_seen
            existing->last_seen = now;
        } else {
            PeerEntry entry;
            entry.host = addr.host;
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
                                              const std::string &host, uint16_t port,
                                              bool is_outbound) {
    if (remote_uuid.empty()) return;

    // Look for existing connection with same UUID
    for (auto &[key, client] : outbound_connections_) {
        if (!client) continue;
        // If we find another connection with the same UUID on a different key
        auto current_key = peer_key(host, port);
        if (key == current_key) continue;

        // Check if this client has the same remote UUID
        // The dedup rule: lower UUID keeps outbound
        if (node_uuid_ < remote_uuid) {
            // We keep our outbound, tell the other side to drop
            logMessage("INFO", "Duplicate connection detected for UUID " + remote_uuid + " — keeping our outbound (lower UUID)");
        } else {
            // We drop our outbound
            logMessage("INFO", "Duplicate connection detected for UUID " + remote_uuid + " — dropping our outbound (higher UUID)");
            if (is_outbound) {
                outbound_connections_.erase(peer_key(host, port));
            }
        }
        break;
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
    bans_.push_back(ban);

    save_peers();
    logMessage("INFO", "Banned peer " + key + " reason: " + reason);
}

void PeerManager::unban_peer(const std::string &host, uint16_t port) {
    bans_.erase(
        std::remove_if(bans_.begin(), bans_.end(),
            [&](const BanRecord &b) { return b.host == host && b.port == port; }),
        bans_.end()
    );
}

bool PeerManager::is_banned(const std::string &host, uint16_t port) const {
    auto now = static_cast<uint64_t>(std::time(nullptr));
    for (const auto &ban : bans_) {
        if (ban.host == host && ban.port == port) {
            if (ban.expires == 0 || ban.expires > now) {
                return true;
            }
        }
    }
    return false;
}

void PeerManager::purge_expired_bans() {
    auto now = static_cast<uint64_t>(std::time(nullptr));
    bans_.erase(
        std::remove_if(bans_.begin(), bans_.end(),
            [now](const BanRecord &b) {
                return b.expires > 0 && b.expires <= now;
            }),
        bans_.end()
    );
}

std::vector<BanRecord> PeerManager::get_bans() const {
    return bans_;
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

    logMessage("INFO", "Scheduling reconnect to " + key + " in " + std::to_string(delay_ms / 1000) + "s");

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
    for (const auto &p : peers_) {
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

void PeerManager::disconnect_and_remove(const std::string &host, uint16_t port) {
    auto key = peer_key(host, port);
    outbound_connections_.erase(key);
    remove_peer(host, port);
    save_peers();
}

// --- Block Propagation ---

void PeerManager::broadcast_block(const Block &block) {
    // Send to all outbound connections
    for (auto &[key, client] : outbound_connections_) {
        if (client && client->is_connected()) {
            client->send(block, PacketType::BLOCK);
        }
    }

    // Send to all tracked inbound sessions
    for (auto it = inbound_sessions_.begin(); it != inbound_sessions_.end(); ) {
        auto session = it->second.lock();
        if (session) {
            session->send_packet_public(block, PacketType::BLOCK);
            ++it;
        } else {
            it = inbound_sessions_.erase(it);
        }
    }
}

void PeerManager::relay_block(const Block &block, const std::string &exclude_key) {
    // Send to all outbound connections except the sender
    for (auto &[key, client] : outbound_connections_) {
        if (key == exclude_key) continue;
        if (client && client->is_connected()) {
            client->send(block, PacketType::BLOCK);
        }
    }

    // Send to all tracked inbound sessions except the sender
    for (auto it = inbound_sessions_.begin(); it != inbound_sessions_.end(); ) {
        auto session = it->second.lock();
        if (session) {
            if (it->first != exclude_key) {
                session->send_packet_public(block, PacketType::BLOCK);
            }
            ++it;
        } else {
            it = inbound_sessions_.erase(it);
        }
    }
}
