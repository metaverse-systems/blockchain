#pragma once

#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>
#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <string>
#include <chrono>
#include <functional>
#include "PeerConfig.hpp"
#include "IBlockchain.hpp"
#include "SyncState.hpp"
#include "json.hpp"

class PeerClient;
class PeerServer;
class BlockPropagation;

class PeerManager {
public:
    PeerManager(boost::asio::io_context &io_context,
                boost::asio::ssl::context &ssl_context,
                const PeerConfig &config,
                const std::filesystem::path &data_dir,
                IBlockchain &bc,
                SyncStatus &sync_status,
                uint16_t p2p_port = 0);

    // Peer list persistence
    void load_peers();
    void save_peers();

    // Peer list management
    bool add_peer(const PeerEntry &entry);
    bool remove_peer(const std::string &host, uint16_t port);
    std::vector<PeerEntry> get_peers() const;
    PeerEntry* find_peer(const std::string &host, uint16_t port);
    const PeerEntry* find_peer(const std::string &host, uint16_t port) const;

    // Self-filtering
    void filter_self(std::vector<PeerAddress> &peers) const;

    // Connection management
    void start();
    void connect_to(const std::string &host, uint16_t port);
    bool can_accept_inbound() const;
    void on_peer_disconnected(const std::string &host, uint16_t port);
    void on_inbound_connected(const std::string &host, uint16_t port, std::shared_ptr<PeerServer> session);
    void on_inbound_disconnected(const std::string &host, uint16_t port);

    // Peer exchange
    void on_peer_exchange_received(const std::string &sender_uuid, uint16_t sender_port,
                                    const std::string &sender_host,
                                    const std::vector<PeerAddress> &received_peers);
    void broadcast_peer_exchange();
    void on_new_peers_discovered(const std::vector<PeerAddress> &new_peers);

    // Duplicate connection detection
    void check_duplicate_connection(const std::string &remote_uuid,
                                     const std::string &host, uint16_t port,
                                     bool is_outbound);

    // Ban management
    void increment_error(const std::string &host, uint16_t port);
    void ban_peer(const std::string &host, uint16_t port, const std::string &reason, uint64_t duration_seconds = 0);
    void unban_peer(const std::string &host, uint16_t port);
    bool is_banned(const std::string &host, uint16_t port) const;
    void purge_expired_bans();
    std::vector<BanRecord> get_bans() const;

    // Reconnection
    void schedule_reconnect(const std::string &host, uint16_t port);

    // Discovery gate
    bool is_discovery_enabled() const { return config_.discovery_enabled; }

    // Accessors
    const std::string& get_node_uuid() const { return node_uuid_; }
    uint16_t get_listen_port() const;
    std::vector<PeerAddress> get_non_banned_peer_addresses() const;
    size_t outbound_count() const;
    size_t inbound_count() const;
    const PeerConfig& get_config() const { return config_; }

    // Disconnect and remove a peer (used by removePeer RPC)
    void disconnect_and_remove(const std::string &host, uint16_t port);

    // Block propagation
    void broadcast_block(const Block &block);
    void relay_block(const Block &block, const std::string &exclude_key);
    void set_block_propagation(BlockPropagation *bp) { block_propagation_ = bp; }

    // Per-peer backoff state
    struct BackoffState {
        uint32_t current_delay = 0;
        std::shared_ptr<boost::asio::steady_timer> timer;
    };

private:
    boost::asio::io_context &io_context_;
    boost::asio::ssl::context &ssl_context_;
    PeerConfig config_;
    std::filesystem::path data_dir_;
    IBlockchain &bc_;
    SyncStatus &sync_status_;
    uint16_t p2p_port_;

    std::string node_uuid_;
    std::vector<PeerEntry> peers_;
    std::vector<BanRecord> bans_;

    // Connection tracking
    std::map<std::string, std::shared_ptr<PeerClient>> outbound_connections_; // "host:port" -> PeerClient
    std::map<std::string, std::weak_ptr<PeerServer>> inbound_sessions_; // "host:port" -> PeerServer
    size_t inbound_count_ = 0;
    BlockPropagation *block_propagation_ = nullptr;

    // Exchange timer
    std::shared_ptr<boost::asio::steady_timer> exchange_timer_;
    void start_exchange_timer();

    // Backoff tracking
    std::map<std::string, BackoffState> backoff_state_;

    // Helper
    static std::string peer_key(const std::string &host, uint16_t port);
    void evict_oldest_peer();
};
