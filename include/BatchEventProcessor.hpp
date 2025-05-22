#pragma once
#include "EventProcessor.hpp"
#include "Sequence.hpp"

namespace disruptor {
    template<typename T>
    class BatchEventProcessor final : public EventProcessor {
        Sequence sequence;
        bool running = false;

    public:
        Sequence &getSequence() const override {
            return this->sequence;
        }

        void halt() override {
            running = false;
        }

        bool isRunning() const override {
            return running;
        }

        void run() {
            running = true;
            this-

                chỗ này dùng sequence barrier clear alert
        }
    };
}
