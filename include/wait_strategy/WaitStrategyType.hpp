#pragma once

/**
 * @file WaitStrategyType.hpp
 * @brief Enum for selecting the consumer wait strategy at compile time.
 *
 * ADAPTIVE:  3-phase wait (spin → yield → sleep). Best overall — balances
 *            latency (~1μs) with low CPU usage. Recommended default.
 * YIELD:     Spin 200 iterations then yield. Higher CPU usage than Adaptive,
 *            slightly lower average latency under sustained load.
 * BUSY_SPIN: Non-stop spinning with PAUSE hint. Lowest possible latency but
 *            100% CPU usage. Can cause thermal throttling under sustained load.
 *            Best for very short bursts where latency is critical.
 */
enum class WaitStrategyType {
    ADAPTIVE,
    YIELD,
    BUSY_SPIN,
};
