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
#include <string>
#include "WaitStrategy.hpp"
#include "Sequence.hpp"
#include "SequenceBarrier.hpp"

namespace disruptor
{

    /**
     * Busy Spin strategy that uses a busy spin loop for EventProcessors waiting on a barrier.
     *
     * This strategy will use CPU resource to avoid syscalls which can introduce latency jitter.  It is best
     * used when threads can be bound to specific CPU cores.
     */
    class BusySpinWaitStrategy : public WaitStrategy
    {
    public:
        [[nodiscard]] int64_t waitFor(const int64_t sequence,
                                      const Sequence &cursor,
                                      SequenceBarrier &barrier) override
        {
            int64_t availableSequence;

            while ((availableSequence = cursor.get()) < sequence)
            {
                barrier.checkAlert();
                std::this_thread::yield(); // Equivalent to Thread.onSpinWait() in Java
            }

            return availableSequence;
        }

        void signalAllWhenBlocking() noexcept override
        {
            // Do nothing, this strategy doesn't block
        }

        [[nodiscard]] std::string toString() const noexcept override
        {
            return "BusySpinWaitStrategy";
        }
    };

} // namespace disruptor