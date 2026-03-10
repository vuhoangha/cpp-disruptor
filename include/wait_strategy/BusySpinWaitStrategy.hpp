#pragma once

#include <string>
#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {
    /**
     * BusySpinWaitStrategy: Spins with CPU hint between checks.
     * Lowest latency but highest CPU usage.
     */
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class BusySpinWaitStrategy final {
    public:
        template<typename Barrier>
        [[nodiscard]] size_t wait_for(const size_t sequence,
                                      SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
                                      const Barrier &barrier) {
            size_t available_sequence;

            while ((available_sequence = dependent_sequences.get()) < sequence) {
                barrier.check_alert();
#if defined(__x86_64__) || defined(__i386__)
                __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
                __asm__ __volatile__("yield");
#endif
            }

            return available_sequence;
        }

        [[nodiscard]] std::string to_string() const noexcept {
            return "BusySpinWaitStrategy";
        }
    };
}
