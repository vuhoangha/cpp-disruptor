#pragma once
#include <functional>
#include <iostream>

#include "EventProcessor.hpp"
#include "Sequence.hpp"
#include "SequenceBarrier.hpp"
#include "AlertException.hpp"
#include "DataProvider.hpp"
#include "RingBuffer.hpp"

namespace disruptor {
    template<typename T>
    class BatchEventProcessor final : public EventProcessor {
        Sequence sequence;
        SequenceBarrier &sequenceBarrier;

        // Định nghĩa kiểu cho function object
        using EventHandler = std::function<void(T &, int64_t, bool)>;
        // Biến lưu trữ function object
        EventHandler eventHandler;

        RingBuffer<T, size_t> &ringBuffer;


    public:
        explicit BatchEventProcessor(SequenceBarrier &barrier, EventHandler handler, RingBuffer<T, size_t> &ringBuffer
        ) : sequenceBarrier(barrier), eventHandler(handler), ringBuffer(ringBuffer) {
        }


        Sequence getSequence() const override {
            return this->sequence.get();
        }


        void halt() override {
            this->sequenceBarrier.alert();
        }


        void run() {
            this->sequenceBarrier.clearAlert();
            processEvents();
        }


        void processEvents() {
            int64_t nextSequence = sequence.get() + 1;

            while (true) {
                try {
                    const int64_t availableSequence = this->sequenceBarrier.waitFor(nextSequence);
                    while (nextSequence <= availableSequence) {
                        T &event = this->ringBuffer.get(nextSequence);
                        this->eventHandler(event, nextSequence, nextSequence == availableSequence);
                        nextSequence++;
                    }

                    this->sequence.set(availableSequence);
                }  catch (const std::exception &e) {
                    std::cout << "BatchEventProcessor exception caught: " << e.what() << std::endl;
                    break;
                }
            }
        }
    };
}
