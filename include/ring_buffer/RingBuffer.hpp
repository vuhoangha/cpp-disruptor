#pragma once

/**
 * @file RingBuffer.hpp
 * @brief Pre-allocated circular buffer — the core data structure of the Disruptor pattern.
 *
 * Key design decisions:
 *   - BUFFER_SIZE must be a power of 2 so that index computation is a single
 *     bitwise AND (sequence & mask) instead of modulo — ~5x faster.
 *   - All entries are pre-allocated at construction time via the event factory.
 *     No allocation happens during publish/consume — zero GC pressure.
 *   - The entries array is padded on both sides to prevent false sharing
 *     with adjacent heap objects.
 *
 * @tparam T          Event type stored in each slot.
 * @tparam BUFFER_SIZE Number of slots. Must be a power of 2.
 */

#include <functional>
#include <array>
#include "../common/Common.hpp"

namespace disruptor {

    template<typename T, size_t BUFFER_SIZE>
    class RingBuffer {
        static_assert(BUFFER_SIZE > 0, "Buffer size must be greater than 0");
        static_assert((BUFFER_SIZE & (BUFFER_SIZE - 1)) == 0, "Buffer size must be a power of 2");

        /// Bitmask for fast modulo: sequence & INDEX_MASK == sequence % BUFFER_SIZE
        static constexpr size_t INDEX_MASK = BUFFER_SIZE - 1;

        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        std::array<T, BUFFER_SIZE> entries;
        char padding_2[CACHE_LINE_SIZE * 2] = {};

        std::function<T()> event_factory;

    public:
        explicit RingBuffer(std::function<T()> event_creator) : event_factory(std::move(event_creator)) {
            for (size_t i = 0; i < BUFFER_SIZE; i++) {
                entries[i] = event_factory();
            }
        }

        /// Get a reference to the event at the given sequence number.
        /// The sequence is masked to wrap around the buffer.
        [[gnu::hot]] [[nodiscard]] T &get(const size_t sequence) noexcept {
            return entries[sequence & INDEX_MASK];
        }

        [[gnu::pure]] [[nodiscard]] static constexpr size_t get_buffer_size() noexcept {
            return BUFFER_SIZE;
        }
    };

} // namespace disruptor
