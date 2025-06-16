#pragma once

#include <thread>
#include <string>
#include "WaitStrategy.hpp"
#include "../barriers/SequenceBarrier.hpp"

namespace disruptor {
    /**
     * Busy Spin strategy that uses a busy spin loop for EventProcessors waiting on a barrier.
     *
     * This strategy will use CPU resource to avoid syscalls which can introduce latency jitter.  It is best
     * used when threads can be bound to specific CPU cores.
     */
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class BusySpinWaitStrategy final : public WaitStrategy<NUMBER_DEPENDENT_SEQUENCES> {
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
            return "BusySpinWaitStrategy";
        }
    };
} // namespace disruptor
