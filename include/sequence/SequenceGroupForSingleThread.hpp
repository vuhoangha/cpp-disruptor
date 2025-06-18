#pragma once

#include <string>
#include <cassert>
#include <format>

#include "Sequence.hpp"

namespace disruptor {
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class SequenceGroupForSingleThread final {
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        size_t cached_min_sequence{0};
        size_t index_min_sequence{0};
        const char padding_2[CACHE_LINE_SIZE - sizeof(size_t) * 2] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

        std::array<Sequence *, NUMBER_DEPENDENT_SEQUENCES> sequences; // Contains an array of pointers to sequences
        const char padding_4[CACHE_LINE_SIZE * 2] = {};

    public:
        explicit SequenceGroupForSingleThread(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            set_sequences(dependent_sequences);
        }

        explicit SequenceGroupForSingleThread() {
            for (std::size_t i = 0; i < NUMBER_DEPENDENT_SEQUENCES; ++i) {
                sequences[i] = nullptr;
            }
        }

        void set_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            assert(dependent_sequences.size() == NUMBER_DEPENDENT_SEQUENCES && std::format("Require {} sequences", NUMBER_DEPENDENT_SEQUENCES).c_str());
            std::size_t i = 0;
            for (auto &ref: dependent_sequences) {
                sequences[i++] = &ref.get();
            }
        }

        // get the most recent cached value
        [[nodiscard]] size_t get_cache() const {
            return cached_min_sequence;
        }

        [[nodiscard]] size_t get() {
            // check if the sequence at the index "index_min_sequence" has changed
            size_t minimum_sequence = sequences[index_min_sequence]->get();
            if (minimum_sequence == cached_min_sequence) {
                return minimum_sequence;
            }

            const size_t old_index = index_min_sequence;
            size_t index = index_min_sequence;
            for (size_t i = 0; i < sequences.size(); ++i) {
                // this sequence has already been retrieved --> skip it
                if (i == old_index)
                    continue;

                const size_t value = sequences[i]->get();

                // another sequence has a value that matches the cached value
                if (value == cached_min_sequence) {
                    index = i;
                    break;
                }

                if (value < minimum_sequence) {
                    minimum_sequence = value;
                    index = i;
                }
            }

            cached_min_sequence = minimum_sequence;
            index_min_sequence = index;

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
        explicit SequenceGroupForSingleThread(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            set_sequences(dependent_sequences);
        }

        explicit SequenceGroupForSingleThread() : sequence(nullptr) {
        }

        void set_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            assert(dependent_sequences.size() == 1 && "Require exactly 1 sequence");
            sequence = &dependent_sequences.begin()->get();
        }

        [[nodiscard]] size_t get() {
            cached_min_sequence = sequence->get();
            return cached_min_sequence;
        }

        // get the most recent cached value
        [[nodiscard]] size_t get_cache() const {
            return cached_min_sequence;
        }
    };
}
