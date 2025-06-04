#pragma once
#include "RingBuffer.hpp"

namespace disruptor {
    template<typename T, size_t N>
    class Disruptor {
        RingBuffer<T> ringBuffer;

    public:
        explicit Disruptor()

    };
}
