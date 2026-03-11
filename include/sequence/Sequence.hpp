#pragma once

/**
 * @file Sequence.hpp
 * @brief Cache-line padded, lock-free sequence counter — the fundamental building block.
 *
 * Memory layout (3 cache lines = 192 bytes):
 *   [64B padding] [value + 56B padding] [64B padding]
 *
 * The padding ensures that no other data shares a cache line with `value`,
 * preventing false sharing between producer and consumer threads.
 *
 * This class deliberately does NOT use std::atomic<size_t> for the value itself.
 * Instead, it uses:
 *   - Standalone memory fences (acquire/release) for single-writer reads/writes
 *   - Inline x86 `lock xaddq` for atomic fetch-and-add (multi-producer path)
 *
 * Why not std::atomic? Two reasons:
 *   1. Standalone fences + plain load/store compile to fewer instructions
 *      than atomic load(acquire)/store(release) on some compilers.
 *   2. `lock xaddq` via inline asm gives us a guaranteed single instruction
 *      for fetch-and-add, whereas std::atomic::fetch_add may emit a CAS loop
 *      depending on compiler/flags.
 */

#include <atomic>
#include "../common/Common.hpp"

namespace disruptor {

    class Sequence final {
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        size_t value;
        const char padding_2[CACHE_LINE_SIZE - sizeof(size_t)] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

    public:
        Sequence() : Sequence(0) {}

        explicit Sequence(const size_t initial_value) {
            set_with_release(initial_value);
        }

        /// Read with acquire fence — guarantees all prior writes by the writer are visible.
        /// Used by consumers reading the producer's cursor.
        [[gnu::hot]] [[nodiscard]] size_t get_with_acquire() const {
            size_t result = value;
            std::atomic_thread_fence(std::memory_order_acquire);
            return result;
        }

        /// Plain read — no fence. Only safe when called from the same thread that writes.
        [[nodiscard]] size_t get() const {
            return value;
        }

        /// Write with release fence — guarantees all prior writes (including ring buffer data)
        /// are visible before this sequence update is seen by consumers.
        [[gnu::hot]] void set_with_release(const size_t newValue) {
            std::atomic_thread_fence(std::memory_order_release);
            value = newValue;
        }

        /// Plain write — no fence. Only safe for single-threaded initialization.
        void set(const size_t newValue) {
            value = newValue;
        }

        /**
         * @brief Atomic fetch-and-add using x86 LOCK XADD instruction.
         *
         * Returns the PREVIOUS value, then atomically adds `increment`.
         * This is the multi-producer hot path — each producer atomically claims
         * a range of sequences by adding N to the shared cursor.
         *
         * LOCK XADD is a single instruction with full memory barrier semantics,
         * making it the fastest possible atomic increment on x86.
         * The `memory` clobber ensures the compiler doesn't reorder surrounding loads/stores.
         */
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

} // namespace disruptor
