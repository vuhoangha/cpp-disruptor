#pragma once

#include <atomic>
#include <string>
#include <cstdint>
#include "Common.hpp"

namespace disruptor
{
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
    class Sequence
    {
    private:
        // The actual sequence value - căn chỉnh theo cache line. Chắc chắn nằm 1 mình 1 cache line
        alignas(CACHE_LINE_SIZE) std::atomic<int64_t> value;

        // Padding sau value để đảm bảo không có biến nào của đối tượng khác nằm trên cùng cache line
        char padding[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>)];

    public:
        static constexpr int64_t INITIAL_VALUE = -1L;

        /**
         * Create a sequence initialised to -1.
         */
        inline Sequence() : Sequence(INITIAL_VALUE) {}

        /**
         * Create a sequence with a specified initial value.
         *
         * @param initialValue The initial value for this sequence.
         */
        inline explicit Sequence(int64_t initialValue)
        {
            // Use release semantics to ensure all previous writes are visible
            value.store(initialValue, std::memory_order_release);
        }

        /**
         * Perform a volatile read of this sequence's value.
         *
         * @return The current value of the sequence.
         */
        [[nodiscard]] virtual inline int64_t get() const
        {
            // Use acquire semantics to ensure all subsequent reads see previous writes
            return value.load(std::memory_order_acquire);
        }

        /**
         * Perform an ordered write of this sequence. The intent is
         * a Store/Store barrier between this write and any previous store.
         *
         * @param newValue The new value for the sequence.
         */
        [[nodiscard]] virtual inline void set(int64_t newValue)
        {
            // Use release semantics to ensure all previous writes are visible
            value.store(newValue, std::memory_order_release);
        }

        /**
         * Performs a volatile write of this sequence. The intent is
         * a Store/Store barrier between this write and any previous
         * write and a Store/Load barrier between this write and any
         * subsequent volatile read.
         *
         * @param newValue The new value for the sequence.
         */
        [[nodiscard]] virtual inline void setVolatile(int64_t newValue)
        {
            // Use sequential consistency for full fence semantics
            value.store(newValue, std::memory_order_seq_cst);
        }

        /**
         * Perform a compare and set operation on the sequence.
         *
         * @param expectedValue The expected current value.
         * @param newValue The value to update to.
         * @return true if the operation succeeds, false otherwise.
         */
        [[nodiscard]] virtual inline bool compareAndSet(int64_t expectedValue, int64_t newValue)
        {
            // Use sequential consistency for full fence semantics
            return value.compare_exchange_strong(expectedValue, newValue,
                                                 std::memory_order_seq_cst);
        }

        /**
         * Atomically increment the sequence by one.
         *
         * @return The value after the increment
         */
        [[nodiscard]] virtual inline int64_t incrementAndGet()
        {
            return addAndGet(1);
        }

        /**
         * Atomically add the supplied value.
         *
         * @param increment The value to add to the sequence.
         * @return The value after the increment.
         */
        [[nodiscard]] virtual inline int64_t addAndGet(int64_t increment)
        {
            // Use sequential consistency for full fence semantics
            return value.fetch_add(increment, std::memory_order_seq_cst) + increment;
        }

        /**
         * Perform an atomic getAndAdd operation on the sequence.
         *
         * @param increment The value to add to the sequence.
         * @return the value before increment
         */
        [[nodiscard]] virtual inline int64_t getAndAdd(int64_t increment)
        {
            // Use sequential consistency for full fence semantics
            return value.fetch_add(increment, std::memory_order_seq_cst);
        }

        /**
         * Convert the sequence to a string.
         */
        [[nodiscard]] virtual inline std::string toString() const
        {
            return std::to_string(get());
        }

        friend std::ostream& operator<<(std::ostream& os, const Sequence& sequence);
    };

} // namespace disruptor