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
    template<typename T, size_t RING_BUFFER_SIZE>
    class AbstractSequencer : public Sequencer {
    protected:
        Sequence cursor{INITIAL_CURSOR_VALUE};
        RingBuffer<T, RING_BUFFER_SIZE> &ringBuffer;

    public:
        explicit AbstractSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ringBuffer)
            : ringBuffer(ringBuffer) {
        }

        /**
         * @see Sequencer#getCursor()
         */
        int64_t getCursor() override {
            return cursor.get();
        }
    };
}
