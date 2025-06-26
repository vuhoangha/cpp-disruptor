#pragma once

#include <atomic>
#include "../common/Common.hpp"

namespace disruptor {
    class Sequence final {
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        size_t value;
        const char padding_2[CACHE_LINE_SIZE - sizeof(size_t)] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

    public:
        Sequence() : Sequence(0) {
        }

        explicit Sequence(const size_t initial_value) {
            set_with_release(initial_value);
        }

        [[gnu::hot]] [[nodiscard]] size_t get_with_acquire() const {
            size_t result = value;
            std::atomic_thread_fence(std::memory_order_acquire);
            return result;
        }

        [[nodiscard]] size_t get() const {
            return value;
        }

        [[gnu::hot]] void set_with_release(const size_t newValue) {
            std::atomic_thread_fence(std::memory_order_release);
            value = newValue;
        }

        void set(const size_t newValue) {
            value = newValue;
        }

        [[gnu::hot]] [[nodiscard]] size_t get_and_add(const size_t increment) {
            size_t origin_value;
            __asm__ __volatile__ (
                "lock xaddq %0, %1"
                : "=r" (origin_value), "+m" (value)
                : "0" (increment)
                : "memory"
            );
            return origin_value;
        }
    };
}
