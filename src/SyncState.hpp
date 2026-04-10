#pragma once

#include <atomic>

enum class SyncState
{
    IDLE,
    SYNCING
};

struct SyncStatus
{
    std::atomic<bool> isSyncing{false};
};
