#pragma once

#include <climits>

namespace disruptor {
    inline constexpr size_t CACHE_LINE_SIZE = 64;

    // Intel L2 prefetcher operates on 128-byte blocks (pairs of cache lines).
    // Synchronization variables should be separated by 128 bytes to prevent
    // false sharing at the L2 level. (Intel Optimization Manual, Rule 18)
    inline constexpr size_t CACHE_LINE_PAIR = 128;

    // Sentinel value returned by wait_for() when the barrier is alerted.
    // Avoids exception throwing/catching on the hot path.
    inline constexpr size_t SEQUENCE_ALERT = SIZE_MAX;
}