// MIT License
#pragma once

#include <atomic>
#include <chrono>
#include <string>
#include <cstdint>

// Forward declarations for gauge computation
class IBlockchain;
class PeerManager;

class MetricsCollector {
public:
    // Atomic counters (monotonic, increment-only)
    std::atomic<uint64_t> rpc_requests_total_{0};
    std::atomic<uint64_t> rpc_errors_total_{0};
    std::atomic<uint64_t> blocks_received_total_{0};
    std::atomic<uint64_t> blocks_rejected_total_{0};

    // Node start time (set once at construction)
    const std::chrono::steady_clock::time_point start_time_ = std::chrono::steady_clock::now();

    // Optional references for gauge computation (set after construction)
    IBlockchain* blockchain_ = nullptr;
    PeerManager* peer_manager_ = nullptr;

    // Generate Prometheus exposition format text (text/plain; version=0.0.4)
    std::string generatePrometheusText() const;

    // Compute uptime in seconds
    double uptime_seconds() const {
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time_).count();
    }
};
