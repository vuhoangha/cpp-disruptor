#pragma once

#include <string>
#include <cassert>
#include <format>

#include "Sequence.hpp"

namespace disruptor {
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class SequenceGroupForSingleThread final {
    private:
        // cache lại min_sequence để tăng performance khi getMinimumSequence
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        int64_t cached_min_sequence{INITIAL_VALUE_SEQUENCE};
        int64_t index_min_sequence{0};
        const char padding2[CACHE_LINE_SIZE - sizeof(int64_t) * 2] = {};
        const char padding3[CACHE_LINE_SIZE] = {};

        std::array<Sequence *, NUMBER_DEPENDENT_SEQUENCES> sequences; // lưu array con trỏ tới các sequence
        const char padding4[CACHE_LINE_SIZE * 2] = {};

    public:
        explicit SequenceGroupForSingleThread(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            this->setSequences(dependentSequences);
        }

        explicit SequenceGroupForSingleThread() {
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

        // trả ra giá trị cache gần nhất
        [[nodiscard]] inline int64_t get_cache() const {
            return cached_min_sequence;
        }


        [[nodiscard]] int64_t get() {
            // kiểm tra xem sequence ở index "index_min_sequence" có thay đổi giá trị ko
            int64_t minimumSequence = this->sequences[index_min_sequence]->get();
            if (minimumSequence == cached_min_sequence) {
                return minimumSequence;
            }

            const size_t old_index = index_min_sequence;
            size_t index = index_min_sequence;
            for (size_t i = 0; i < this->sequences.size(); ++i) {
                // sequence này đã lấy value --> bỏ qua
                if (i == old_index) continue;

                const int64_t value = this->sequences[i].get();

                // sequence khác có value trùng với value cache
                if (value == cached_min_sequence) {
                    index = i;
                    break;
                }

                if (value < minimumSequence) {
                    minimumSequence = value;
                    index = i;
                }
            }

            cached_min_sequence = minimumSequence;
            index_min_sequence = index;

            return minimumSequence;
        }
    };


    template<>
    class SequenceGroupForSingleThread<1> final {
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        Sequence *sequence;
        const char padding2[CACHE_LINE_SIZE - sizeof(void *)] = {};
        const char padding3[CACHE_LINE_SIZE] = {};

        // cache lại min_sequence để tăng performance
        int64_t cached_min_sequence{INITIAL_VALUE_SEQUENCE};
        const char padding4[CACHE_LINE_SIZE - sizeof(int64_t)] = {};
        const char padding5[CACHE_LINE_SIZE] = {};

    public:
        explicit SequenceGroupForSingleThread(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            setSequences(dependentSequences);
        }

        explicit SequenceGroupForSingleThread() : sequence(nullptr) {
        }

        void setSequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            assert(dependentSequences.size() == 1 && "Require exactly 1 sequence");
            sequence = &dependentSequences.begin()->get();
        }

        [[nodiscard]] int64_t get() {
            cached_min_sequence = sequence->get();
            return cached_min_sequence;
        }

        // trả ra giá trị cache gần nhất
        [[nodiscard]] inline int64_t get_cache() const {
            return cached_min_sequence;
        }
    };
}
