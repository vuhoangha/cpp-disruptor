#pragma once

/**
 * @file SequenceGroupForSingleThread.hpp
 * @brief Tracks the minimum sequence across multiple consumers (single-threaded access only).
 *
 * Used by SingleProducerSequencer to find the slowest consumer (gating sequence).
 * The producer needs this to know how far it can advance without overwriting
 * unconsumed events in the ring buffer.
 *
 * Optimization: caches the minimum value and the index of the slowest consumer.
 * On each call to get(), it first checks if the previously-slowest consumer has
 * advanced. If not, the cached minimum is still valid — avoids scanning all sequences.
 *
 * Template specialization for N=1 (single consumer) eliminates the array and scan entirely.
 */

#include <cassert>
#include <array>
#include <limits>

#include "Sequence.hpp"

namespace disruptor {

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class SequenceGroupForSingleThread final {
        // --- Cached minimum: on its own cache line to avoid false sharing ---
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        size_t value_min_sequence_cache{0};   ///< Last known minimum sequence value
        size_t index_min_sequence_cache{0};   ///< Index of the consumer that had the minimum
        const char padding_2[CACHE_LINE_SIZE - sizeof(size_t) * 2] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

        std::array<Sequence *, NUMBER_DEPENDENT_SEQUENCES> sequences;
        const char padding_4[CACHE_LINE_SIZE * 2] = {};

        /// Full scan of all consumer sequences to find the minimum.
        void calculate_cache() {
            size_t minimum_sequence = std::numeric_limits<size_t>::max();
            size_t minimum_index = 0;
            for (size_t k = 0; k < sequences.size(); k++) {
                const size_t value = sequences[k]->get_with_acquire();
                if (value < minimum_sequence) {
                    minimum_sequence = value;
                    minimum_index = k;
                }
            }
            value_min_sequence_cache = minimum_sequence;
            index_min_sequence_cache = minimum_index;
        }

    public:
        explicit SequenceGroupForSingleThread(
            const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            set_sequences(dependent_sequences);
        }

        explicit SequenceGroupForSingleThread() {
            for (std::size_t i = 0; i < NUMBER_DEPENDENT_SEQUENCES; ++i) {
                sequences[i] = nullptr;
            }
        }

        void set_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            assert(dependent_sequences.size() == NUMBER_DEPENDENT_SEQUENCES);
            std::size_t i = 0;
            for (auto &ref: dependent_sequences) {
                sequences[i++] = &ref.get();
            }
            calculate_cache();
        }

        /// Return the cached minimum without any atomic load. Only valid as a hint.
        [[gnu::hot]] [[nodiscard]] size_t get_cache() const {
            return value_min_sequence_cache;
        }

        /**
         * @brief Get the current minimum sequence across all consumers.
         *
         * Fast path: if the previously-slowest consumer hasn't changed, return immediately.
         * Medium path: if another consumer now matches the cached min, update index and return.
         * Slow path: full scan to recompute the minimum.
         */
        [[gnu::hot]] [[nodiscard]] size_t get() {
            // Fast path: check only the previously-slowest consumer
            if (value_min_sequence_cache == sequences[index_min_sequence_cache]->get_with_acquire()) [[likely]] {
                return value_min_sequence_cache;
            }

            // The slowest consumer advanced — need to find the new minimum
            size_t index = 0;
            size_t minimum_sequence = std::numeric_limits<size_t>::max();
            for (size_t i = 0; i < sequences.size(); i++) {
                const size_t value = sequences[i]->get_with_acquire();

                // Medium path: another consumer is at the same position as old minimum
                if (value == value_min_sequence_cache) {
                    index_min_sequence_cache = i;
                    return value_min_sequence_cache;
                }

                if (value < minimum_sequence) {
                    minimum_sequence = value;
                    index = i;
                }
            }

            value_min_sequence_cache = minimum_sequence;
            index_min_sequence_cache = index;

            return minimum_sequence;
        }
    };

    /**
     * @brief Specialization for single consumer — no array, no scan.
     * Simply reads the one consumer's sequence directly.
     */
    template<>
    class SequenceGroupForSingleThread<1> final {
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        Sequence *sequence;
        const char padding_2[CACHE_LINE_SIZE - sizeof(void *)] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

        size_t cached_min_sequence{0};
        const char padding_4[CACHE_LINE_SIZE - sizeof(size_t)] = {};
        const char padding_5[CACHE_LINE_SIZE] = {};

    public:
        explicit SequenceGroupForSingleThread(
            const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            set_sequences(dependent_sequences);
        }

        explicit SequenceGroupForSingleThread() : sequence(nullptr) {}

        void set_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            assert(dependent_sequences.size() == 1 && "Require exactly 1 sequence");
            sequence = &dependent_sequences.begin()->get();
            cached_min_sequence = sequence->get();
        }

        [[gnu::hot]] [[nodiscard]] size_t get() {
            cached_min_sequence = sequence->get_with_acquire();
            return cached_min_sequence;
        }

        [[gnu::hot]] [[nodiscard]] size_t get_cache() const {
            return cached_min_sequence;
        }
    };

} // namespace disruptor
