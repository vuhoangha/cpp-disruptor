#pragma once

#include <string>
#include <cassert>
#include <stdexcept>
#include <format>
#include <atomic>

#include "Sequence.hpp"
#include "Util.hpp"

namespace disruptor {
    /**
     * Hides a group of Sequences behind a single Sequence
     */
    template<size_t N>
    class FixedSequenceGroup final : public Sequence {
    private:
        // cache lại min_sequence để tăng performance khi getMinimumSequence
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE];
        std::atomic<int64_t> cached_min_sequence{Sequence::INITIAL_VALUE};
        const char padding2[CACHE_LINE_SIZE - sizeof(std::atomic<int64_t>)];
        const char padding3[CACHE_LINE_SIZE];

        std::array<Sequence *, N> sequences; // lưu array con trỏ tới các sequence
        const char padding4[CACHE_LINE_SIZE * 2];

    public:
        explicit FixedSequenceGroup(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            this->setSequences(dependentSequences);
        }

        explicit FixedSequenceGroup() {
            for (std::size_t i = 0; i < N; ++i) {
                sequences[i] = nullptr;
            }
        }


        // Phương thức để thiết lập sequences sau khi khởi tạo
        void setSequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            assert(dependentSequences.size() == N && std::format("Require {} sequences", N).c_str());
            std::size_t i = 0;
            for (auto &ref: dependentSequences) {
                sequences[i++] = &ref.get();
            }
        }


        [[nodiscard]] inline int64_t get() override {
            const int64_t min_sequence = cached_min_sequence.load(std::memory_order_relaxed);
            const int64_t new_min_sequence = Util::getMinimumSequenceWithCache(sequences, min_sequence);
            if (min_sequence < new_min_sequence) {
                cached_min_sequence.store(new_min_sequence, std::memory_order_relaxed);
            }
            return new_min_sequence;
        }


        /**
         * Not supported.
         */
        [[noreturn]] void set(int64_t value) override {
            throw std::runtime_error("FixedSequenceGroup does not support set operation");
        }

        /**
         * Not supported.
         */
        [[noreturn]] bool compareAndSet(int64_t expectedValue, int64_t newValue) override {
            throw std::runtime_error("FixedSequenceGroup does not support compareAndSet operation");
        }

        /**
         * Not supported.
         */
        [[noreturn]] int64_t incrementAndGet() override {
            throw std::runtime_error("FixedSequenceGroup does not support incrementAndGet operation");
        }

        /**
         * Not supported.
         */
        [[noreturn]] int64_t addAndGet(int64_t increment) override {
            throw std::runtime_error("FixedSequenceGroup does not support addAndGet operation");
        }
    };


    template<>
    class FixedSequenceGroup<1> final : public Sequence {
    private:
        alignas(CACHE_LINE_SIZE) const char padding1[CACHE_LINE_SIZE];
        Sequence *sequence;
        const char padding2[CACHE_LINE_SIZE - sizeof(void *)];
        const char padding3[CACHE_LINE_SIZE];

    public:
        explicit FixedSequenceGroup(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            setSequences(dependentSequences);
        }

        explicit FixedSequenceGroup() : sequence(nullptr) {
        }

        void setSequences(const std::initializer_list<std::reference_wrapper<Sequence> > dependentSequences) {
            assert(dependentSequences.size() == 1 && "Require exactly 1 sequence");
            sequence = &dependentSequences.begin()->get();
        }


        [[nodiscard]] inline int64_t get() override {
            return sequence->get();
        }


        /**
         * Not supported.
         */
        [[noreturn]] void set(int64_t value) override {
            throw std::runtime_error("FixedSequenceGroup does not support set operation");
        }

        /**
         * Not supported.
         */
        [[noreturn]] bool compareAndSet(int64_t expectedValue, int64_t newValue) override {
            throw std::runtime_error("FixedSequenceGroup does not support compareAndSet operation");
        }

        /**
         * Not supported.
         */
        [[noreturn]] int64_t incrementAndGet() override {
            throw std::runtime_error("FixedSequenceGroup does not support incrementAndGet operation");
        }

        /**
         * Not supported.
         */
        [[noreturn]] int64_t addAndGet(int64_t increment) override {
            throw std::runtime_error("FixedSequenceGroup does not support addAndGet operation");
        }
    };
} // namespace disruptor
