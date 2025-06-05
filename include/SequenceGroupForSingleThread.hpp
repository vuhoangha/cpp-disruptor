#pragma once

#include <string>
#include <cassert>
#include <stdexcept>
#include <format>

#include "Sequence.hpp"

namespace disruptor {
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class SequenceGroupForSingleThread final : public Sequence {
    private:
        // cache lại min_sequence để tăng performance khi getMinimumSequence
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        int64_t cached_min_sequence{Sequence::INITIAL_VALUE};
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


        [[nodiscard]] int64_t get() override {
            // kiểm tra xem sequence ở index "index_min_sequence" có thay đổi giá trị ko
            int64_t minimumSequence = this->sequences[index_min_sequence]->get();
            if (minimumSequence == cached_min_sequence) {
                return minimumSequence;
            }

            const int64_t old_index = index_min_sequence;
            int64_t index = index_min_sequence;
            for (std::size_t i = 0; i < this->sequences.size(); ++i) {
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


        [[noreturn]] void set(int64_t value) override {
            throw std::runtime_error("SequenceGroupForSingleThread does not support set operation");
        }

        [[noreturn]] bool compareAndSet(int64_t expectedValue, int64_t newValue) override {
            throw std::runtime_error("SequenceGroupForSingleThread does not support compareAndSet operation");
        }

        [[noreturn]] int64_t incrementAndGet() override {
            throw std::runtime_error("SequenceGroupForSingleThread does not support incrementAndGet operation");
        }

        [[noreturn]] int64_t addAndGet(int64_t increment) override {
            throw std::runtime_error("SequenceGroupForSingleThread does not support addAndGet operation");
        }
    };


    template<>
    class SequenceGroupForSingleThread<1> final : public Sequence {
    private:
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE] = {};
        Sequence *sequence;
        const char padding2[CACHE_LINE_SIZE - sizeof(void *)] = {};
        const char padding3[CACHE_LINE_SIZE] = {};

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

        [[nodiscard]] inline int64_t get() override {
            return sequence->get();
        }

        [[noreturn]] void set(int64_t value) override {
            throw std::runtime_error("SequenceGroupForSingleThread does not support set operation");
        }

        [[noreturn]] bool compareAndSet(int64_t expectedValue, int64_t newValue) override {
            throw std::runtime_error("SequenceGroupForSingleThread does not support compareAndSet operation");
        }

        [[noreturn]] int64_t incrementAndGet() override {
            throw std::runtime_error("SequenceGroupForSingleThread does not support incrementAndGet operation");
        }

        [[noreturn]] int64_t addAndGet(int64_t increment) override {
            throw std::runtime_error("SequenceGroupForSingleThread does not support addAndGet operation");
        }
    };
}
