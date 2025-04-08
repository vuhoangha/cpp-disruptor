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

namespace disruptor {

/**
 * Yielding strategy that uses a Thread.yield() for EventProcessors waiting on a barrier
 * after an initially spinning.
 *
 * This strategy will use 100% CPU, but will more readily give up the CPU than a busy spin strategy if other threads
 * require CPU resource.
 */
class YieldingWaitStrategy : public WaitStrategy
{
private:
    static constexpr int SPIN_TRIES = 100;

    inline int applyWaitMethod(SequenceBarrier& barrier, const int counter)
    {
        barrier.checkAlert();

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
    int64_t waitFor(
        const int64_t sequence, const Sequence& cursor, const Sequence& dependentSequence, SequenceBarrier& barrier) override
    {
        int64_t availableSequence;
        int counter = SPIN_TRIES;

        while ((availableSequence = dependentSequence.get()) < sequence)
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
        return "YieldingWaitStrategy";
    }
};

} // namespace disruptor 