#pragma once

#include <thread>
#include <string>
#include "WaitStrategy.hpp"
#include "Sequence.hpp"
#include "SequenceBarrier.hpp"

namespace disruptor {
    /**
     * Busy Spin strategy that uses a busy spin loop for EventProcessors waiting on a barrier.
     *
     * This strategy will use CPU resource to avoid syscalls which can introduce latency jitter.  It is best
     * used when threads can be bound to specific CPU cores.
     */
    class BusySpinWaitStrategy final : public WaitStrategy {
    public:
        [[nodiscard]] int64_t waitFor(const int64_t sequence,
                                      Sequence &cursor,
                                      const SequenceBarrier &barrier) override {
            int64_t availableSequence;

            while ((availableSequence = cursor.get()) < sequence) {
                barrier.checkAlert();
                std::this_thread::yield();
            }

            return availableSequence;
        }

        [[nodiscard]] std::string toString() const noexcept override {
            return "BusySpinWaitStrategy";
        }
    };
} // namespace disruptor
