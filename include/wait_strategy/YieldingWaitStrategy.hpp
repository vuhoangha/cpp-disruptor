#pragma once

#include <thread>
#include <string>
#include "WaitStrategy.hpp"
#include "../barriers/SequenceBarrier.hpp"

namespace disruptor
{
    /**
     * Yielding strategy that uses a Thread.yield() for EventProcessors waiting on a barrier
     * after an initially spinning.
     *
     * This strategy will use 100% CPU, but will more readily give up the CPU than a busy spin strategy if other threads
     * require CPU resource.
     */
    template <size_t NUMBER_DEPENDENT_SEQUENCES>
    class YieldingWaitStrategy final : public WaitStrategy<NUMBER_DEPENDENT_SEQUENCES>
    {
        static constexpr int SPIN_TRIES = 100;

        static int apply_wait_method(const SequenceBarrier &barrier, const int counter)
        {
            barrier.check_alert();

            if (0 == counter)
            {
                std::this_thread::yield();
            }
            else
            {
                return counter - 1;
            }

            return counter;
        }

    public:
        [[nodiscard]] size_t wait_for(const size_t sequence,
                                      SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
                                      const SequenceBarrier &barrier) override
        {
            size_t available_sequence;
            int counter = SPIN_TRIES;

            while ((available_sequence = dependent_sequences.get()) < sequence)
            {
                counter = apply_wait_method(barrier, counter);
            }

            return available_sequence;
        }

        [[nodiscard]] std::string to_string() const noexcept override
        {
            return "YieldingWaitStrategy";
        }
    };
}
