#pragma once

#include "AbstractSequencer.hpp"

namespace disruptor {
    class MultiProducerSequencer : public AbstractSequencer {
    private:

        // seq nhỏ nhất đã được các gatingSequences xử lý
        alignas(CACHE_LINE_SIZE) std::atomic<int64_t> cachedValue;

        đang dở chỗ này

        char padding[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>) * 2];


    public:
        MultiProducerSequencer(int bufferSize, std::shared_ptr<WaitStrategy> waitStrategy) {

        }
    };
}