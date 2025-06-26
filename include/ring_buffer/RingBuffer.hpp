#pragma once

#include <functional>
#include <array>
#include "../common/Common.hpp"

namespace disruptor {
    template<typename T, size_t BUFFER_SIZE>
    class RingBuffer {
        static_assert(BUFFER_SIZE > 0, "Buffer size must be greater than 0");
        static_assert((BUFFER_SIZE & (BUFFER_SIZE - 1)) == 0, "Buffer size must be a power of 2");

        static constexpr size_t INDEX_MASK = BUFFER_SIZE - 1;

        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        std::array<T, BUFFER_SIZE> entries;
        char padding_2[CACHE_LINE_SIZE * 2] = {};

        std::function<T()> event_factory;

    public:
        explicit RingBuffer(std::function<T()> event_creator) : event_factory(std::move(event_creator)) {
            // Pre-populate the buffer with events
            for (size_t i = 0; i < BUFFER_SIZE; i++) {
                entries[i] = event_factory();
            }
        }

        [[gnu::hot]] [[nodiscard]] T &get(const size_t sequence) noexcept {
            return entries[sequence & INDEX_MASK];
        }

        [[gnu::pure]] [[nodiscard]] static constexpr size_t get_buffer_size() noexcept {
            return BUFFER_SIZE;
        }
    };
}
