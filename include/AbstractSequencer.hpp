#pragma once

#include <vector>
#include <memory>
#include <stdexcept>
#include <bit>

#include "Sequencer.hpp"
#include "Sequence.hpp"
#include "ProcessingSequenceBarrier.hpp"
#include "Util.hpp"

namespace disruptor
{

    /**
     * Base class for the various sequencer types (single/multi). Provides
     * common functionality like the management of gating sequences (add/remove) and
     * ownership of the current cursor.
     */
    class AbstractSequencer : public Sequencer
    {
    protected:
        Sequence cursor{INITIAL_CURSOR_VALUE};
        const int bufferSize;
        std::vector<std::shared_ptr<Sequence>> gatingSequences;

    public:
        explicit AbstractSequencer(const int bufferSize)
            : bufferSize(bufferSize)
        {
            if (bufferSize < 1)
            {
                throw std::invalid_argument("bufferSize must not be less than 1");
            }
            if (std::popcount(static_cast<unsigned int>(bufferSize)) != 1)
            {
                throw std::invalid_argument("bufferSize must be a power of 2");
            }
        }


        /**
         * @see Sequencer#getCursor()
         */
        int64_t getCursor() override
        {
            return cursor.get();
        }

        /**
         * @see Sequencer#getBufferSize()
         */
        int getBufferSize() const override
        {
            return bufferSize;
        }

        /**
         * @see Sequencer#addGatingSequences(Sequence...)
         */
        void addGatingSequences(const std::vector<std::shared_ptr<Sequence>> &sequences) override
        {
            // Lấy giá trị cursor hiện tại
            const int64_t cursorSequence = getCursor();

            // Dự trữ không gian cho vector
            gatingSequences.reserve(gatingSequences.size() + sequences.size());

            // Thiết lập giá trị và thêm các sequence mới
            for (const auto &sequence : sequences)
            {
                sequence->set(cursorSequence);
                gatingSequences.push_back(sequence);
            }
        }


        /**
         * @see Sequencer#getMinimumSequence()
         */
        int64_t getMinimumSequence() override
        {
            return Util::getMinimumSequence(gatingSequences, cursor.get());
        }
    };

}