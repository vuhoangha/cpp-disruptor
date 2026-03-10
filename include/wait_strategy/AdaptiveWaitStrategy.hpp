#pragma once

#include "../common/Util.hpp"
#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class AdaptiveWaitStrategy final {
    public:
        template<typename Barrier>
        [[gnu::hot]] [[nodiscard]] size_t wait_for(const size_t sequence,
                                      SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
                                      const Barrier &barrier) {
            size_t available_sequence;
            int wait_counter = 0;

            while ((available_sequence = dependent_sequences.get()) < sequence) {
                barrier.check_alert();
                Util::adaptive_wait(wait_counter);
            }

            return available_sequence;
        }

    };
}
