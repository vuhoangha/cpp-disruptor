#pragma once

#include <string>
#include <cassert>
#include <format>
#include <atomic>

#include "Sequence.hpp"

namespace disruptor {
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class SequenceGroupForMultiThread final {
    private:
        // cache lại min_sequence để tăng performance khi get_minimum_sequence
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        std::atomic<int64_t> cached_min_sequence{Sequence::INITIAL_VALUE};
        const char padding2[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>)] = {};
        const char padding3[CACHE_LINE_SIZE] = {};

        std::array<Sequence *, NUMBER_DEPENDENT_SEQUENCES> sequences; // lưu array con trỏ tới các sequence
        const char padding4[CACHE_LINE_SIZE * 2] = {};

    public:
        explicit SequenceGroupForMultiThread(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            this->setSequences(dependentSequences);
        }

        explicit SequenceGroupForMultiThread() {
            for (std::size_t i = 0; i < NUMBER_DEPENDENT_SEQUENCES; ++i) {
                sequences[i] = nullptr;
            }
        }

        void setSequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            assert(dependentSequences.size() == NUMBER_DEPENDENT_SEQUENCES && std::format("Require {} sequences", NUMBER_DEPENDENT_SEQUENCES).c_str());
            std::size_t i = 0;
            for (auto &ref: dependentSequences) {
                sequences[i++] = &ref.get();
            }
        }

        [[nodiscard]] int64_t get() {
            const int64_t min_sequence = this->cached_min_sequence.load(std::memory_order_relaxed);
            const int64_t new_min_sequence = get_minimum_sequence(min_sequence);
            if (min_sequence < new_min_sequence) {
                this->cached_min_sequence.store(new_min_sequence, std::memory_order_relaxed);
            }
            return new_min_sequence;
        }

        [[nodiscard]] int64_t get_minimum_sequence(const int64_t cached_sequence) {
            int64_t minimumSequence = INT64_MAX;
            for (const auto &sequence: this->sequences) {
                const int64_t value = sequence->get();
                if (value <= cached_sequence) {
                    return cached_sequence;
                }
                minimumSequence = std::min(minimumSequence, value);
            }
            return minimumSequence;
        }

        // trả ra giá trị cache gần nhất
        [[nodiscard]] int64_t get_cache() const {
            return this->cached_min_sequence.load(std::memory_order_relaxed);
        }
    };


    template<>
    class SequenceGroupForMultiThread<1> final {
    private:
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        Sequence *sequence;
        const char padding2[CACHE_LINE_SIZE - sizeof(void *)] = {};
        const char padding3[CACHE_LINE_SIZE] = {};

    public:
        explicit SequenceGroupForMultiThread(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            setSequences(dependentSequences);
        }

        explicit SequenceGroupForMultiThread() : sequence(nullptr) {
        }

        void setSequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            assert(dependentSequences.size() == 1 && "Require exactly 1 sequence");
            sequence = &dependentSequences.begin()->get();
        }

        [[nodiscard]] inline int64_t get() const {
            return sequence->get();
        }

        [[nodiscard]] int64_t get_cache() const {
            return sequence->getRelax();
        }
    };
}
