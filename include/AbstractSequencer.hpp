#pragma once

#include <vector>
#include <memory>

#include "Sequencer.hpp"
#include "Sequence.hpp"
#include "ProcessingSequenceBarrier.hpp"
#include "RingBuffer.hpp"
#include "Util.hpp"

namespace disruptor {
    /**
     * Base class for the various sequencer types (single/multi). Provides
     * common functionality like the management of gating sequences (add/remove) and
     * ownership of the current cursor.
     */
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class AbstractSequencer : public Sequencer {
    protected:
        Sequence cursor{INITIAL_CURSOR_VALUE};
        FixedSequenceGroup<NUMBER_GATING_SEQUENCES> gatingSequences;
        RingBuffer<T, RING_BUFFER_SIZE> &ringBuffer;

    public:
        explicit AbstractSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ringBuffer)
            : ringBuffer(ringBuffer), bufferSize(ringBuffer.getBufferSize()) {
        }


        /**
         * @see Sequencer#getCursor()
         */
        int64_t getCursor() override {
            return cursor.get();
        }

        /**
         * @see Sequencer#addGatingSequences(Sequence...)
         */
        void addGatingSequences(const std::vector<std::shared_ptr<Sequence> > &sequences) override {
            // Lấy giá trị cursor hiện tại
            const int64_t cursorSequence = getCursor();

            // Dự trữ không gian cho vector
            gatingSequences.reserve(gatingSequences.size() + sequences.size());

            // Thiết lập giá trị và thêm các sequence mới
            for (const auto &sequence: sequences) {
                sequence->set(cursorSequence);
                gatingSequences.push_back(sequence);
            }
        }


        /**
         * @see Sequencer#getMinimumSequence()
         */
        int64_t getMinimumSequence() override {
            return Util::getMinimumSequence(gatingSequences, cursor.get());
        }
    };
}
