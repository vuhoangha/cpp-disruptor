#pragma once

#include <thread>
#include <string>
#include "WaitStrategy.hpp"
#include "SequenceBarrier.hpp"

namespace disruptor {
    /**
     * Yielding strategy that uses a Thread.yield() for EventProcessors waiting on a barrier
     * after an initially spinning.
     *
     * This strategy will use 100% CPU, but will more readily give up the CPU than a busy spin strategy if other threads
     * require CPU resource.
     */
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class YieldingWaitStrategy final : public WaitStrategy<NUMBER_DEPENDENT_SEQUENCES> {
    private:
        static constexpr int SPIN_TRIES = 100;

        static int applyWaitMethod(const SequenceBarrier &barrier, const int counter) {
            barrier.checkAlert();

            if (0 == counter) {
                std::this_thread::yield();
            } else {
                return counter - 1;
            }

            return counter;
        }

    public:
        [[nodiscard]] int64_t waitFor(const int64_t sequence,
                                      SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
                                      const SequenceBarrier &barrier) override {
            int64_t availableSequence;
            int counter = SPIN_TRIES;

            while ((availableSequence = dependent_sequences.get()) < sequence) {
                counter = applyWaitMethod(barrier, counter);
            }

            return availableSequence;
        }


        [[nodiscard]] std::string toString() const noexcept override {
            return "YieldingWaitStrategy";
        }
    };
} // namespace disruptor
