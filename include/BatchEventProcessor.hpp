#pragma once
#include "EventProcessor.hpp"
#include "Sequence.hpp"

namespace disruptor {
    template<typename T>
    class BatchEventProcessor : public EventProcessor {
        Sequence sequence = new Sequence(Sequence::INITIAL_VALUE);

    public:
        Sequence getSequence() const override {
            return this->sequence;
        }
    };
}
