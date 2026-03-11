#pragma once

/**
 * @file AdaptiveWaitStrategy.hpp
 * @brief 3-phase wait: spin (100 PAUSE) → yield (10x) → sleep (1ns loop).
 *
 * Delegates the actual phase logic to Util::adaptive_wait().
 * This strategy provides the best throughput in benchmarks across all scenarios
 * because it avoids both thermal throttling (BusySpin) and excessive context
 * switching (Yield under low contention).
 */

#include "../common/Util.hpp"
#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class AdaptiveWaitStrategy final {
    public:
        template<typename Barrier>
        [[gnu::hot]] [[nodiscard]] size_t wait_for(const size_t sequence,
                                      SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
                                      const Barrier &barrier) noexcept {
            size_t available_sequence;
            int wait_counter = 0;

            while ((available_sequence = dependent_sequences.get()) < sequence) {
                if (barrier.is_alerted()) [[unlikely]] return SEQUENCE_ALERT;
                Util::adaptive_wait(wait_counter);
            }

            return available_sequence;
        }
    };

} // namespace disruptor
