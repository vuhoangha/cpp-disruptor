#pragma once

#include <string>
#include <cassert>
#include <format>
#include <atomic>

#include "Sequence.hpp"

namespace disruptor {
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class SequenceGroupForMultiThread final {
        // cache lại min_sequence để tăng performance khi get_minimum_sequence
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        std::atomic<size_t> cached_min_sequence{0};
        const char padding_2[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

        std::array<Sequence *, NUMBER_DEPENDENT_SEQUENCES> sequences; // lưu array con trỏ tới các sequence
        const char padding_4[CACHE_LINE_SIZE * 2] = {};

    public:
        explicit SequenceGroupForMultiThread(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            this->set_sequences(dependent_sequences);
        }

        explicit SequenceGroupForMultiThread() {
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

        [[nodiscard]] size_t get() {
            const size_t min_sequence = this->cached_min_sequence.load(std::memory_order_relaxed);
            const size_t new_min_sequence = get_minimum_sequence(min_sequence);
            if (min_sequence < new_min_sequence) {
                this->cached_min_sequence.store(new_min_sequence, std::memory_order_relaxed);
            }
            return new_min_sequence;
        }

        [[nodiscard]] size_t get_minimum_sequence(const size_t cached_sequence) {
            size_t minimum_sequence = INT64_MAX;
            for (const auto &sequence: this->sequences) {
                const size_t value = sequence->get();
                if (value <= cached_sequence) {
                    return cached_sequence;
                }
                minimum_sequence = std::min(minimum_sequence, value);
            }
            return minimum_sequence;
        }

        // trả ra giá trị cache gần nhất
        [[nodiscard]] size_t get_cache() const {
            return this->cached_min_sequence.load(std::memory_order_relaxed);
        }
    };

    template<>
    class SequenceGroupForMultiThread<1> final {
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        Sequence *sequence;
        const char padding_2[CACHE_LINE_SIZE - sizeof(void *)] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

    public:
        explicit SequenceGroupForMultiThread(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            set_sequences(dependent_sequences);
        }

        explicit SequenceGroupForMultiThread() : sequence(nullptr) {
        }

        void set_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences) {
            assert(dependent_sequences.size() == 1 && "Require exactly 1 sequence");
            sequence = &dependent_sequences.begin()->get();
        }

        [[nodiscard]] size_t get() const {
            return sequence->get();
        }

        [[nodiscard]] size_t get_cache() const {
            return sequence->get_relax();
        }
    };
}
