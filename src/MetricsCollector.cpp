// MIT License
#include "MetricsCollector.hpp"
#include "IBlockchain.hpp"
#include "PeerManager.hpp"
#include <sstream>
#include <iomanip>

std::string MetricsCollector::generatePrometheusText() const {
    std::ostringstream oss;

    // Gauges from component state
    int64_t chain_height = 0;
    if (blockchain_) {
        chain_height = static_cast<int64_t>(blockchain_->getChainLength());
    }

    int64_t peer_count = 0;
    if (peer_manager_) {
        peer_count = static_cast<int64_t>(peer_manager_->get_peers().size());
    }

    int64_t chunk_count = 0;
    if (blockchain_) {
        chunk_count = static_cast<int64_t>(blockchain_->getChunkCount());
    }

    int64_t active_connections = 0;
    if (peer_manager_) {
        active_connections = static_cast<int64_t>(peer_manager_->outbound_count() + peer_manager_->inbound_count());
    }

    double uptime = uptime_seconds();

    // blockchain_chain_height (gauge)
    oss << "# HELP blockchain_chain_height Current blockchain height.\n"
        << "# TYPE blockchain_chain_height gauge\n"
        << "blockchain_chain_height " << chain_height << "\n";

    // blockchain_peer_count (gauge)
    oss << "# HELP blockchain_peer_count Number of currently connected peers.\n"
        << "# TYPE blockchain_peer_count gauge\n"
        << "blockchain_peer_count " << peer_count << "\n";

    // blockchain_chunk_count (gauge)
    oss << "# HELP blockchain_chunk_count Number of chunks on disk.\n"
        << "# TYPE blockchain_chunk_count gauge\n"
        << "blockchain_chunk_count " << chunk_count << "\n";

    // blockchain_active_connections (gauge)
    oss << "# HELP blockchain_active_connections Number of active P2P connections.\n"
        << "# TYPE blockchain_active_connections gauge\n"
        << "blockchain_active_connections " << active_connections << "\n";

    // blockchain_uptime_seconds (gauge)
    oss << "# HELP blockchain_uptime_seconds Seconds since node start.\n"
        << "# TYPE blockchain_uptime_seconds gauge\n"
        << std::fixed << std::setprecision(1)
        << "blockchain_uptime_seconds " << uptime << "\n";

    // blockchain_rpc_requests_total (counter)
    oss << "# HELP blockchain_rpc_requests_total Total RPC requests processed.\n"
        << "# TYPE blockchain_rpc_requests_total counter\n"
        << "blockchain_rpc_requests_total " << rpc_requests_total_.load() << "\n";

    // blockchain_rpc_errors_total (counter)
    oss << "# HELP blockchain_rpc_errors_total Total RPC errors returned.\n"
        << "# TYPE blockchain_rpc_errors_total counter\n"
        << "blockchain_rpc_errors_total " << rpc_errors_total_.load() << "\n";

    // blockchain_blocks_received_total (counter)
    oss << "# HELP blockchain_blocks_received_total Total blocks received from P2P.\n"
        << "# TYPE blockchain_blocks_received_total counter\n"
        << "blockchain_blocks_received_total " << blocks_received_total_.load() << "\n";

    // blockchain_blocks_rejected_total (counter)
    oss << "# HELP blockchain_blocks_rejected_total Total blocks rejected by validation.\n"
        << "# TYPE blockchain_blocks_rejected_total counter\n"
        << "blockchain_blocks_rejected_total " << blocks_rejected_total_.load() << "\n";

    return oss.str();
}
