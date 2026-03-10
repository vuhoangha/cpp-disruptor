#pragma once

#include <climits>

namespace disruptor {
    inline constexpr size_t CACHE_LINE_SIZE = 64;

    // Sentinel value returned by wait_for() when the barrier is alerted.
    // Avoids exception throwing/catching on the hot path.
    inline constexpr size_t SEQUENCE_ALERT = SIZE_MAX;
}