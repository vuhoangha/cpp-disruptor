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
#include "AlertException.hpp"

namespace disruptor
{

    /**
     * SequenceBarrier handed out for gating EventProcessors on a cursor sequence and optional dependent EventProcessor(s),
     * using the given WaitStrategy.
     */
    class ProcessingSequenceBarrier : public SequenceBarrier
    {
    private:
        WaitStrategy &waitStrategy;
        Sequence &dependentSequence;
        std::atomic<bool> alerted;
        Sequence &cursorSequence;
        Sequencer &sequencer;
        std::unique_ptr<FixedSequenceGroup> ownedSequenceGroup; // biến này để chứa lại object FixedSequenceGroup được tạo ra, tránh nó bị destructor làm lỗi biến "dependentSequence"

    public:
        ProcessingSequenceBarrier(
            Sequencer &sequencer,
            WaitStrategy &waitStrategy,
            Sequence &cursorSequence,
            const std::vector<std::shared_ptr<Sequence>> &dependentSequences)
            : waitStrategy(waitStrategy),
              alerted(false),
              cursorSequence(cursorSequence),
              sequencer(sequencer),
              // Khởi tạo ownedSequenceGroup trước
              ownedSequenceGroup(dependentSequences.empty() ? nullptr : std::make_unique<FixedSequenceGroup>(dependentSequences)),
              // Sau đó khởi tạo dependentSequence để tham chiếu đến đối tượng thích hợp
              dependentSequence(dependentSequences.empty() ? cursorSequence : static_cast<Sequence &>(*ownedSequenceGroup))
        {
        }

        int64_t waitFor(int64_t sequence) override
        {
            checkAlert();
            int64_t availableSequence = waitStrategy.waitFor(sequence, cursorSequence, dependentSequence, *this);

            if (availableSequence < sequence)
            {
                return availableSequence;
            }

            return sequencer.getHighestPublishedSequence(sequence, availableSequence);
        }

        int64_t getCursor() const override
        {
            return dependentSequence.get();
        }

        bool isAlerted() const override
        {
            return alerted.load(std::memory_order_acquire);
        }

        void alert() override
        {
            alerted.store(true, std::memory_order_release);
            waitStrategy.signalAllWhenBlocking();
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