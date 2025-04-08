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

#include <atomic>
#include <memory>
#include <vector>
#include "SequenceBarrier.hpp"
#include "WaitStrategy.hpp"
#include "Sequence.hpp"
#include "Sequencer.hpp"
#include "FixedSequenceGroup.hpp"

namespace disruptor
{

    /**
     * SequenceBarrier handed out for gating EventProcessors on a cursor sequence and optional dependent EventProcessor(s),
     * using the given WaitStrategy.
     */
    class ProcessingSequenceBarrier : public SequenceBarrier
    {
    private:
        std::unique_ptr<WaitStrategy> waitStrategy;
        std::shared_ptr<Sequence> dependentSequence;
        std::atomic<bool> alerted;
        std::shared_ptr<Sequence> cursorSequence;
        std::shared_ptr<Sequencer> sequencer;

    public:
        ProcessingSequenceBarrier(
            std::shared_ptr<Sequencer> sequencer,
            std::unique_ptr<WaitStrategy> waitStrategy,
            std::shared_ptr<Sequence> cursorSequence,
            const std::vector<std::shared_ptr<Sequence>> &dependentSequences)
            : waitStrategy(std::move(waitStrategy)),
              alerted(false),
              cursorSequence(cursorSequence),
              sequencer(sequencer)
        {
            if (dependentSequences.empty())
            {
                dependentSequence = cursorSequence;
            }
            else
            {
                dependentSequence = std::make_shared<FixedSequenceGroup>(dependentSequences);
            }
        }

        int64_t waitFor(int64_t sequence) override
        {
            checkAlert();




            giờ phải fix mấy lỗi này





            int64_t availableSequence = waitStrategy->waitFor(
                sequence, cursorSequence.get(), dependentSequence.get(), this);

            if (availableSequence < sequence)
            {
                return availableSequence;
            }

            return sequencer->getHighestPublishedSequence(sequence, availableSequence);
        }

        int64_t getCursor() const override
        {
            return dependentSequence->get();
        }

        bool isAlerted() const override
        {
            return alerted.load(std::memory_order_acquire);
        }

        void alert() override
        {
            alerted.store(true, std::memory_order_release);
            waitStrategy->signalAllWhenBlocking();
        }

        void clearAlert() override
        {
            alerted.store(false, std::memory_order_release);
        }

        void checkAlert() const override
        {
            if (alerted.load(std::memory_order_acquire))
            {
                throw AlertException();
            }
        }
    };

} // namespace disruptor