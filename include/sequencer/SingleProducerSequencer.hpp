#pragma once

#include "Sequencer.hpp"
#include "../common/Common.hpp"
#include "../common/Util.hpp"
#include <unordered_map>
#include <cassert>

#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class SingleProducerSequencer final : public Sequencer {
        // quản lý các sequence đã được publish
        alignas(CACHE_LINE_SIZE) Sequence cursor{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};

        // seq gần nhất đã được publisher claim
        size_t latest_claimed_sequence{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};
        const char padding_1[CACHE_LINE_SIZE - sizeof(size_t)] = {};
        const char padding_2[CACHE_LINE_SIZE] = {};

        const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer;
        SequenceGroupForSingleThread<NUMBER_GATING_SEQUENCES> gating_sequences;

        bool same_thread() {
            return ProducerThreadAssertion::is_same_thread_producing_to(this);
        }

    public:
        explicit SingleProducerSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer) : ring_buffer(ring_buffer) {
        }

        void add_gating_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > sequences) override {
            this->gating_sequences.set_sequences(sequences);
        }

        size_t next() override {
            return this->next(1);
        }

        size_t next(const size_t n) override {
            assert(same_thread() && "Accessed by two threads - use ProducerType.MULTI!");
            const size_t buffer_size = this->ring_buffer.get_buffer_size();

            if (n < 1 || n > buffer_size) {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const size_t local_next_value = this->latest_claimed_sequence;
            const size_t next_sequence = local_next_value + n;
            const size_t wrap_point = next_sequence - buffer_size;

            if (this->gating_sequences.get_cache() < wrap_point) {
                // chờ cho tới khi consumer xử lý xong để có chỗ trống ghi dữ liệu
                while (wrap_point > this->gating_sequences.get()) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
            }

            this->latest_claimed_sequence = next_sequence;

            return next_sequence;
        }

        void claim(const size_t sequence) override {
            this->latest_claimed_sequence = sequence;
        }

        void publish(const size_t sequence) override {
            this->cursor.set(sequence);
        }

        void publish(const size_t lo, const size_t hi) override {
            this->publish(hi);
        }

        [[nodiscard]] bool is_available(const size_t sequence) const override {
            const size_t current_sequence = this->cursor.get();
            return sequence <= current_sequence && sequence > current_sequence - this->ring_buffer.get_buffer_size();
        }

        // trong các sequence barrier thì thường "nextSequence" là sequence mà barrier đang chờ, availableSequence là sequence đã được publish hoặc các dependency consmer xử lý xong
        // trong hàm waitFor của SequenceBarrier thì nextSequence <= availableSequence thì mới gọi tới đây
        [[nodiscard]] size_t get_highest_published_sequence(const size_t next_sequence, const size_t available_sequence) const override {
            return available_sequence;
        }

        [[nodiscard]] Sequence &getCursor() {
            return cursor;
        }

        /**
         * Only used when assertions are enabled.
         */
        class ProducerThreadAssertion {
            /**
             * Tracks the threads publishing to SingleProducerSequencers to identify if more than one
             * thread accesses any SingleProducerSequencer.
             * I.e. it helps developers detect early if they use the wrong ProducerType.
             */
            static inline std::unordered_map<SingleProducerSequencer *, std::thread::id> PRODUCERS;
            static inline std::mutex producers_mutex;

        public:
            static bool is_same_thread_producing_to(SingleProducerSequencer *single_producer_sequencer) {
                std::lock_guard lock(producers_mutex);

                const std::thread::id currentThread = std::this_thread::get_id();
                if (!PRODUCERS.contains(single_producer_sequencer)) {
                    PRODUCERS[single_producer_sequencer] = currentThread;
                }

                return PRODUCERS[single_producer_sequencer] == currentThread;
            }
        };
    };
}
