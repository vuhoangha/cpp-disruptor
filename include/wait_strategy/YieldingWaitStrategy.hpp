#pragma once

#include <string>
#include "WaitStrategy.hpp"
#include "../barriers/SequenceBarrier.hpp"
#include "Util.hpp"

namespace disruptor {
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class YieldingWaitStrategy final : public WaitStrategy<NUMBER_DEPENDENT_SEQUENCES> {
    public:
        [[nodiscard]] size_t wait_for(const size_t sequence,
                                      SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
                                      const SequenceBarrier &barrier) override {
            size_t available_sequence;

            while ((available_sequence = dependent_sequences.get()) < sequence) {
                barrier.check_alert();
                std::this_thread::yield();
            }

            return available_sequence;
        }

        [[nodiscard]] std::string to_string() const noexcept override {
            return "YieldingWaitStrategy";
        }
    };
}
