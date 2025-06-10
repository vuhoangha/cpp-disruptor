#pragma once
#include <functional>
#include <iostream>

#include "Sequence.hpp"
#include "SequenceBarrier.hpp"
#include "AlertException.hpp"
#include "RingBuffer.hpp"
#include "Util.hpp"

namespace disruptor {
    template<typename T, size_t BUFFER_SIZE>
    class BatchEventProcessor final {
        Sequence sequence;
        SequenceBarrier &sequenceBarrier;

        // Định nghĩa kiểu cho function object
        using EventHandler = std::function<void(T &, size_t, bool)>;
        // Biến lưu trữ function object
        EventHandler eventHandler;

        RingBuffer<T, BUFFER_SIZE> &ringBuffer;

    public:
        explicit BatchEventProcessor(SequenceBarrier &barrier, EventHandler handler, RingBuffer<T, BUFFER_SIZE> &ringBuffer
        ) : sequence(Util::calculate_initial_value_sequence(ringBuffer.getBufferSize())),
            sequenceBarrier(barrier),
            eventHandler(handler),
            ringBuffer(ringBuffer) {
        }


        [[nodiscard]] Sequence &getCursor() {
            return this->sequence;
        }


        void halt() const {
            this->sequenceBarrier.alert();
        }


        void run() {
            this->sequenceBarrier.clearAlert();
            processEvents();
        }


        void processEvents() {
            size_t nextSequence = sequence.get() + 1;

            while (true) {
                try {
                    const size_t availableSequence = this->sequenceBarrier.waitFor(nextSequence);
                    while (nextSequence <= availableSequence) {
                        T &event = this->ringBuffer.get(nextSequence);
                        this->eventHandler(event, nextSequence, nextSequence == availableSequence);
                        nextSequence++;
                    }

                    this->sequence.set(availableSequence);
                } catch (const std::exception &e) {
                    std::cout << "BatchEventProcessor exception caught: " << e.what() << std::endl;
                    break;
                }
            }
        }
    };
}
