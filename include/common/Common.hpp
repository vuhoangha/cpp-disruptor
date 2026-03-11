#pragma once

/**
 * @file Common.hpp
 * @brief Core constants used across the entire Disruptor++ library.
 *
 * These values define hardware-aware alignment and sentinel constants
 * that are fundamental to the lock-free design.
 */

#include <climits>

namespace disruptor {

    /// Standard x86/ARM cache line size (64 bytes).
    /// All hot fields are aligned to this boundary to prevent false sharing.
    inline constexpr size_t CACHE_LINE_SIZE = 64;

    /// Intel L2 spatial prefetcher fetches pairs of cache lines (128 bytes).
    /// Synchronization variables should be separated by at least this distance
    /// to prevent false sharing at the L2 level.
    /// Reference: Intel Optimization Manual, Section 2.4.5 (Rule 18).
    inline constexpr size_t CACHE_LINE_PAIR = 128;

    /// Sentinel value returned by wait_for() when the barrier is alerted (halt requested).
    /// Using SIZE_MAX avoids exception throwing/catching on the hot path —
    /// consumers simply check for this value instead.
    inline constexpr size_t SEQUENCE_ALERT = SIZE_MAX;

} // namespace disruptor