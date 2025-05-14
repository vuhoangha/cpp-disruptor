#pragma once

#include "AbstractSequencer.hpp"
#include "Common.hpp"

namespace disruptor
{
    class SingleProducerSequencer : public AbstractSequencer
    {
    private:
        // sắp xếp 2 biến này vào 1 cache line riêng vì chúng thường được dùng cùng nhau, cùng 1 thread
        // seq gần nhất đã được publisher claim
        alignas(CACHE_LINE_SIZE) int64_t nextValue{INITIAL_CURSOR_VALUE};
        // seq nhỏ nhất đã được các gatingSequences xử lý
        int64_t cachedValue{INITIAL_CURSOR_VALUE};
        // Padding sau value để đảm bảo không có biến nào của đối tượng khác nằm trên cùng cache line
        char padding[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>) * 2];
    };
}