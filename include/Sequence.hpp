#pragma once

#include <atomic>
#include <string>
#include "Common.hpp"

namespace disruptor {
    /**
     * đảm bảo rằng mỗi đối tượng của class này sẽ bắt đầu tại địa chỉ bộ nhớ là bội số của CACHE_LINE_SIZE (64 bytes)
     * khi đó đối tượng này sẽ nằm trong 1 cache line riêng biệt, không bị false sharing
     * 
     * Concurrent sequence class used for tracking the progress of
     * the ring buffer and event processors. Support a number
     * of concurrent operations including CAS and order writes.
     *
     * Also attempts to be more efficient with regards to false
     * sharing by adding padding around the atomic field.
     */
    class Sequence final {
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        std::atomic<size_t> value;
        const char padding2[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)] = {};
        const char padding3[CACHE_LINE_SIZE] = {};

    public:
        /**
         * Create a sequence initialised to -1.
         */
        Sequence() : Sequence(INITIAL_VALUE_SEQUENCE) {
        }

        /**
         * Create a sequence with a specified initial value.
         *
         * @param initialValue The initial value for this sequence.
         */
        explicit Sequence(const size_t initialValue) {
            // Use release semantics to ensure all previous writes are visible
            value.store(initialValue, std::memory_order_release);
        }

        /**
         * Perform a volatile read of this sequence's value.
         *
         * @return The current value of the sequence.
         */
        [[nodiscard]] size_t get() const {
            // Use acquire semantics to ensure all subsequent reads see previous writes
            return value.load(std::memory_order_acquire);
        }

        [[nodiscard]] size_t getRelax() const {
            return value.load(std::memory_order_relaxed);
        }

        /**
         * Perform an ordered write of this sequence. The intent is
         * a Store/Store barrier between this write and any previous store.
         *
         * @param newValue The new value for the sequence.
         */
        void set(const size_t newValue) {
            // Use release semantics to ensure all previous writes are visible
            value.store(newValue, std::memory_order_release);
        }

        void setRelax(const size_t newValue) {
            value.store(newValue, std::memory_order_relaxed);
        }

        /**
         * Perform a compare and set operation on the sequence.
         *
         * @param expectedValue The expected current value.
         * @param newValue The value to update to.
         * @return true if the operation succeeds, false otherwise.
         */
        [[nodiscard]] bool compareAndSet(size_t expectedValue, const size_t newValue) {
            // Use sequential consistency for full fence semantics
            return value.compare_exchange_strong(expectedValue, newValue,
                                                 std::memory_order_seq_cst);
        }

        /**
         * Atomically increment the sequence by one.
         *
         * @return The value after the increment
         */
        [[nodiscard]] size_t incrementAndGet() {
            return addAndGet(1);
        }

        /**
         * Atomically add the supplied value.
         *
         * @param increment The value to add to the sequence.
         * @return The value after the increment.
         */
        [[nodiscard]] size_t addAndGet(const size_t increment) {
            // Use sequential consistency for full fence semantics
            return value.fetch_add(increment, std::memory_order_seq_cst) + increment;
        }

        /**
         * Perform an atomic getAndAdd operation on the sequence.
         *
         * @param increment The value to add to the sequence.
         * @return the value before increment
         */
        [[nodiscard]] size_t getAndAddRelax(const size_t increment) {
            return value.fetch_add(increment, std::memory_order_relaxed);
        }

        /**
         * Convert the sequence to a string.
         */
        [[nodiscard]] std::string toString() const {
            return std::to_string(get());
        }

        friend std::ostream &operator<<(std::ostream &os, const Sequence &sequence);
    };
}
