#pragma once

#include <string>
#include <cassert>
#include <array>
#include <format>

#include "Sequence.hpp"

namespace disruptor {
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class SequenceGroupForSingleThread final {
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        size_t value_min_sequence_cache{0};
        size_t index_min_sequence_cache{0};
        const char padding_2[CACHE_LINE_SIZE - sizeof(size_t) * 2] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

        std::array<Sequence *, NUMBER_DEPENDENT_SEQUENCES> sequences; // Contains an array of pointers to sequences
        const char padding_4[CACHE_LINE_SIZE * 2] = {};

        void calculate_cache() {
            size_t minimum_sequence = INT64_MAX;
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
            assert(
                dependent_sequences.size() == NUMBER_DEPENDENT_SEQUENCES && std::format("Require {} sequences",
                    NUMBER_DEPENDENT_SEQUENCES).c_str());
            std::size_t i = 0;
            for (auto &ref: dependent_sequences) {
                sequences[i++] = &ref.get();
            }

            calculate_cache();
        }

        // get the most recent cached value
        [[nodiscard]] size_t get_cache() const {
            return value_min_sequence_cache;
        }

        [[nodiscard]] size_t get() {
            // check if the sequence at the index "index_min_sequence" has changed
            if (value_min_sequence_cache == sequences[index_min_sequence_cache]->get_with_acquire()) {
                return value_min_sequence_cache;
            }

            size_t index = 0;
            size_t minimum_sequence = INT64_MAX;
            for (size_t i = 0; i < sequences.size(); i++) {
                const size_t value = sequences[i]->get_with_acquire();

                // another sequence has a value that matches the cached value
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

        explicit SequenceGroupForSingleThread() : sequence(nullptr) {
        }

        void set_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            assert(dependent_sequences.size() == 1 && "Require exactly 1 sequence");
            sequence = &dependent_sequences.begin()->get();
            cached_min_sequence = sequence->get_relaxxxx();
        }

        [[nodiscard]] size_t get() {
            cached_min_sequence = sequence->get_with_acquire();
            return cached_min_sequence;
        }

        // get the most recent cached value
        [[nodiscard]] size_t get_cache() const {
            return cached_min_sequence;
        }
    };
}
