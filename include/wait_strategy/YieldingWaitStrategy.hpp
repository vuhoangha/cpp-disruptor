#pragma once

#include <thread>
#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {
    /**
     * YieldingWaitStrategy: Spin for SPIN_TRIES iterations, then yield.
     * Matches LMAX Java's YieldingWaitStrategy behavior.
     * Good balance between latency and CPU usage.
     */
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
}
