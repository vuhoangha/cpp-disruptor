#pragma once

/**
 * @file BusySpinWaitStrategy.hpp
 * @brief Non-stop spinning with PAUSE/YIELD hint. Lowest latency, highest CPU usage.
 *
 * WARNING: Under sustained load, this strategy burns 100% CPU on the consumer core,
 * which can cause thermal throttling and actually REDUCE throughput compared to
 * Adaptive. Best suited for ultra-low latency requirements with bursty workloads
 * where the consumer is expected to catch up quickly.
 */

#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class BusySpinWaitStrategy final {
    public:
        template<typename Barrier>
        [[gnu::hot]] [[nodiscard]] size_t wait_for(const size_t sequence,
                                      SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
                                      const Barrier &barrier) noexcept {
            size_t available_sequence;

            while ((available_sequence = dependent_sequences.get()) < sequence) {
                if (barrier.is_alerted()) [[unlikely]] return SEQUENCE_ALERT;
#if defined(__x86_64__) || defined(__i386__)
                __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
                __asm__ __volatile__("yield");
#endif
            }

            return available_sequence;
        }
    };

} // namespace disruptor
