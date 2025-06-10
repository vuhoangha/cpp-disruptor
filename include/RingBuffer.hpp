#pragma once

#include <functional>
#include <array>
#include "Common.hpp"

// chứa bộ đệm vòng với padding kỹ lưỡng
namespace disruptor {
    template<typename T, size_t BUFFER_SIZE>
    class RingBuffer {
        static_assert(BUFFER_SIZE > 0, "Buffer size must be greater than 0");
        static_assert((BUFFER_SIZE & (BUFFER_SIZE - 1)) == 0, "Buffer size must be a power of 2");

        static constexpr size_t INDEX_MASK = BUFFER_SIZE - 1;

        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        std::array<T, BUFFER_SIZE> entries;
        char padding2[CACHE_LINE_SIZE * 2] = {};

        std::function<T()> eventFactory;

    public:
        explicit RingBuffer(std::function<T()> eventFactory) : eventFactory(std::move(eventFactory)) {
            // Pre-populate the buffer with events
            for (size_t i = 0; i < BUFFER_SIZE; i++) {
                entries[i] = this->eventFactory();
            }
        }

        T &get(const size_t sequence) noexcept {
            return entries[sequence & INDEX_MASK];
        }

        static constexpr size_t getBufferSize() noexcept {
            return BUFFER_SIZE;
        }
    };
}
