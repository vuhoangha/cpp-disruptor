#pragma once

/**
 * @file YieldingWaitStrategy.hpp
 * @brief Spin 200 iterations with PAUSE hint, then yield the thread.
 *
 * Matches the LMAX Java Disruptor's YieldingWaitStrategy behavior.
 * After exhausting spin tries, falls back to std::this_thread::yield()
 * which gives up the CPU timeslice but doesn't sleep.
 *
 * Trade-off: lower average latency than Adaptive under sustained load,
 * but higher CPU usage (no sleep phase). In practice, Adaptive often
 * wins on throughput due to avoiding thermal throttling.
 */

#include <thread>
#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class YieldingWaitStrategy final {
        static constexpr int SPIN_TRIES = 200;

    public:
        template<typename Barrier>
        [[gnu::hot]] [[nodiscard]] size_t wait_for(const size_t sequence,
                                      SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
                                      const Barrier &barrier) noexcept {
            size_t available_sequence;
            int counter = SPIN_TRIES;

            while ((available_sequence = dependent_sequences.get()) < sequence) {
                if (barrier.is_alerted()) [[unlikely]] return SEQUENCE_ALERT;
                if (counter > 0) {
                    --counter;
#if defined(__x86_64__) || defined(__i386__)
                    __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
                    __asm__ __volatile__("yield");
#endif
                } else {
                    std::this_thread::yield();
                }
            }

            return available_sequence;
        }
    };

} // namespace disruptor
