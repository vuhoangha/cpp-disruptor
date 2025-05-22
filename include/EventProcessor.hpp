#pragma once
#include "Sequence.hpp"

namespace disruptor {
    class EventProcessor {
    public:
        virtual ~EventProcessor() = default;

        // lấy sequence gần nhất đã xử lý
        virtual const Sequence& getSequence() const = 0;


        /**
         * Signal that this EventProcessor should stop when it has finished consuming at the next clean break.
         * It will call {@link SequenceBarrier#alert()} to notify the thread to check status.
         */
        virtual void halt() = 0;


        /**
         * @return whether this event processor is running or not
         * Implementations should ideally return false only when the associated thread is idle.
         */
        virtual bool isRunning() const = 0;
    };
}
