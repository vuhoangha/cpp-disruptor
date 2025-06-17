#pragma once

#include <atomic>
#include <string>
#include "../common/Common.hpp"

namespace disruptor {
    class Sequence final {
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        std::atomic<size_t> value;
        const char padding_2[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

    public:
        Sequence() : Sequence(0) {
        }

        explicit Sequence(const size_t initial_value) {
            value.store(initial_value, std::memory_order_release);
        }

        [[nodiscard]] size_t get() const {
            return value.load(std::memory_order_acquire);
        }

        [[nodiscard]] size_t get_relax() const {
            return value.load(std::memory_order_relaxed);
        }

        void set(const size_t newValue) {
            value.store(newValue, std::memory_order_release);
        }

        void set_relax(const size_t newValue) {
            value.store(newValue, std::memory_order_relaxed);
        }

        [[nodiscard]] size_t get_and_add_relax(const size_t increment) {
            return value.fetch_add(increment, std::memory_order_relaxed);
        }

        [[nodiscard]] std::string to_string() const {
            return std::to_string(get());
        }

        friend std::ostream &operator<<(std::ostream &os, const Sequence &sequence);
    };
}
