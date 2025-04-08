#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include <string>
#include "WaitStrategy.hpp"
#include "Sequence.hpp"
#include "SequenceBarrier.hpp"
#include "AlertException.hpp"

namespace disruptor
{

    /**
     * Blocking strategy that uses a lock and condition variable for EventProcessors waiting on a barrier.
     *
     * This strategy can be used when throughput and low-latency are not as important as CPU resource.
     */
    class BlockingWaitStrategy : public WaitStrategy
    {

    private:
        std::mutex mutex_;
        std::condition_variable condition_;

    public:
        /**
         * Wait for the given sequence to be available.
         *
         * @param sequence          sequence to be waited on.
         * @param cursor            the main sequence from ringbuffer.
         * @param dependent_sequence on which to wait.
         * @param barrier           the processor is waiting on.
         * @return the sequence that is available which may be greater than the requested sequence.
         */
        [[nodiscard]] int64_t waitFor(const int64_t sequence,
                                      const Sequence &cursor,
                                      const Sequence &dependent_sequence,
                                      SequenceBarrier &barrier) override
        {
            int64_t available_sequence;
            if (cursor.get() < sequence)
            {
                std::unique_lock<std::mutex> lock(mutex_);
                while (cursor.get() < sequence)
                {
                    barrier.checkAlert();
                    condition_.wait(lock);
                }
            }

            while ((available_sequence = dependent_sequence.get()) < sequence)
            {
                barrier.checkAlert();
                std::this_thread::yield();
            }

            return available_sequence;
        }

        /**
         * Signal all waiting EventProcessors that the cursor has advanced.
         */
        void signalAllWhenBlocking() noexcept override
        {
            std::lock_guard<std::mutex> lock(mutex_);
            condition_.notify_all();
        }

        /**
         * String representation of the wait strategy
         */
        [[nodiscard]] std::string toString() const noexcept override
        {
            return "BlockingWaitStrategy";
        }
    };

} // namespace disruptor