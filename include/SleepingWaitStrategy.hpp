/*
 * Copyright 2011 LMAX Ltd.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include <thread>
#include <chrono>
#include <string>
#include "WaitStrategy.hpp"
#include "Sequence.hpp"
#include "SequenceBarrier.hpp"

namespace disruptor
{

    /**
     * Sleeping strategy that initially spins, then uses a Thread.yield(), and
     * eventually sleep (std::this_thread::sleep_for) for the minimum
     * number of nanos the OS and JVM will allow while the
     * EventProcessors are waiting on a barrier.
     *
     * This strategy is a good compromise between performance and CPU resource.
     * Latency spikes can occur after quiet periods. It will also reduce the impact
     * on the producing thread as it will not need signal any conditional variables
     * to wake up the event handling thread.
     */
    class SleepingWaitStrategy : public WaitStrategy
    {
    private:
        static const int SPIN_THRESHOLD = 100;
        static const int DEFAULT_RETRIES = 200;
        static const int64_t DEFAULT_SLEEP = 100;

        const int retries;
        const int64_t sleepTimeNs;

        int applyWaitMethod(SequenceBarrier &barrier, const int counter)
        {
            barrier.checkAlert();

            if (counter > SPIN_THRESHOLD)
            {
                return counter - 1;
            }
            else if (counter > 0)
            {
                std::this_thread::yield();
                return counter - 1;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::nanoseconds(sleepTimeNs));
            }

            return counter;
        }

    public:
        /**
         * Provides a sleeping wait strategy with the default retry and sleep settings
         */
        SleepingWaitStrategy() : SleepingWaitStrategy(DEFAULT_RETRIES, DEFAULT_SLEEP)
        {
        }

        /**
         * @param retries How many times the strategy should retry before sleeping
         */
        explicit SleepingWaitStrategy(const int retries) : SleepingWaitStrategy(retries, DEFAULT_SLEEP)
        {
        }

        /**
         * @param retries How many times the strategy should retry before sleeping
         * @param sleepTimeNs How long the strategy should sleep, in nanoseconds
         */
        SleepingWaitStrategy(const int retries, const int64_t sleepTimeNs) : retries(retries), sleepTimeNs(sleepTimeNs)
        {
        }

        [[nodiscard]] int64_t waitFor(const int64_t sequence,
                                      const Sequence &cursor,
                                      const Sequence &dependent_sequence,
                                      SequenceBarrier &barrier) override
        {
            int64_t availableSequence;
            int counter = retries;

            while ((availableSequence = dependent_sequence.get()) < sequence)
            {
                counter = applyWaitMethod(barrier, counter);
            }

            return availableSequence;
        }

        void signalAllWhenBlocking() noexcept override
        {
            // Do nothing, this strategy doesn't block
        }

        [[nodiscard]] std::string toString() const noexcept override
        {
            return "SleepingWaitStrategy";
        }
    };

} // namespace disruptor