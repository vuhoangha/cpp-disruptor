#pragma once

#include "../sequence/SequenceGroupForMultiThread.hpp"
#include "Sequencer.hpp"
#include "common/Util.hpp"
#include "ring_buffer/RingBuffer.hpp"

/**
 * cursor: the highest sequence number that has been claimed by the producer but not yet published.
 * availableBuffer: store the corresponding rotation count for the position in the ring buffer to determine whether a sequence has been published.
 */
namespace disruptor {
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class MultiProducerSequencer final : public Sequencer {
        alignas(CACHE_LINE_SIZE) Sequence cursor{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};

        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        const size_t index_mask;
        const size_t index_shift; // ring_buffer = 16 --> indexShift = 4 (log2(16) = 4). Sequence = 89 --> 89 >> 4 = 5. To reach this sequence, the ring buffer must complete 5 full rotations.
        const char padding_2[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>) * 2] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

        std::array<Sequence, RING_BUFFER_SIZE> available_buffer;
        const char padding_4[CACHE_LINE_SIZE * 2] = {};

        const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer;
        SequenceGroupForMultiThread<NUMBER_GATING_SEQUENCES> gating_sequences;

    public:
        explicit MultiProducerSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer_ptr)
            : index_mask(ring_buffer_ptr.get_buffer_size() - 1), index_shift(Util::log_2(ring_buffer_ptr.get_buffer_size())), ring_buffer(ring_buffer_ptr) {
            for (auto &seq: available_buffer) {
                seq.set(-1);
            }
        }

        void add_gating_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > sequences) override {
            gating_sequences.set_sequences(sequences);
        }

        size_t next() override {
            return next(1);
        }

        size_t next(const size_t n) override {
            const size_t buffer_size = ring_buffer.get_buffer_size();

            if (n < 1 || n > buffer_size) {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const size_t current_sequence = cursor.get_and_add_relax(n);
            const size_t next_sequence = current_sequence + n;
            const size_t wrap_point = next_sequence - buffer_size;
            const size_t cached_gating_sequence = gating_sequences.get_cache();

            if (cached_gating_sequence < wrap_point) {
                while (gating_sequences.get() < wrap_point) {
                    std::this_thread::sleep_for(std::chrono::nanoseconds(1));
                }
            }

            return next_sequence;
        }

        void publish(const size_t sequence) override {
            set_available(sequence);
        }

        void publish(const size_t low, const size_t high) override {
            for (size_t i = low; i <= high; ++i) {
                set_available(i);
            }
        }

        void set_available(const size_t sequence) {
            const size_t index = calculate_index(sequence);
            const size_t flag = calculate_availability_flag(sequence);
            available_buffer[index].set(flag);
        }

        [[nodiscard]] size_t calculate_availability_flag(const size_t sequence) const {
            return sequence >> index_shift;
        }

        [[nodiscard]] size_t calculate_index(const size_t sequence) const {
            return sequence & index_mask;
        }

        [[nodiscard]] bool is_available(const size_t sequence) const override {
            const size_t index = calculate_index(sequence);
            const size_t flag = calculate_availability_flag(sequence);
            return available_buffer[index].get() == flag;
        }

        [[nodiscard]] Sequence &get_cursor() {
            return cursor;
        }

        /**
         * Retrieve the highest sequence that has been published for the consumer to process.
         * In a multi-producer environment, it's possible that sequence 10 has already been published by producer A, while sequence 9, handled by producer B, is still being processed.
         */
        [[nodiscard]] size_t get_highest_published_sequence(const size_t lower_bound, const size_t available_sequence) const override {
            for (size_t sequence = lower_bound; sequence <= available_sequence; ++sequence) {
                if (!is_available(sequence)) {
                    return sequence - 1;
                }
            }
            return available_sequence;
        }
    };
}
