#pragma once

#include "../sequence/SequenceGroupForMultiThread.hpp"
#include "../common/Util.hpp"
#include "../ring_buffer/RingBuffer.hpp"

/**
 * cursor: the highest sequence number that has been claimed by the producer but not yet published.
 * availableBuffer: store the corresponding rotation count for the position in the ring buffer to determine whether a sequence has been published.
 */
namespace disruptor {
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class MultiProducerSequencer final {
        alignas(CACHE_LINE_SIZE) Sequence cursor{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};

        // Cached minimum gating sequence — shared hint across producer threads.
        // Uses relaxed atomic: stale reads only cause an extra slow-path check, never incorrectness.
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> cached_gating_sequence{0};
        const char padding_0[CACHE_LINE_SIZE - sizeof(std::atomic<size_t>)] = {};

        alignas(CACHE_LINE_SIZE) const size_t index_mask;
        const size_t index_shift;
        const char padding_2[CACHE_LINE_SIZE - sizeof(size_t) * 2] = {};
        const char padding_3[CACHE_LINE_SIZE] = {};

        std::array<Sequence, RING_BUFFER_SIZE> available_buffer;
        const char padding_4[CACHE_LINE_SIZE * 2] = {};

        const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer;
        SequenceGroupForMultiThread<NUMBER_GATING_SEQUENCES> gating_sequences;

    public:
        explicit MultiProducerSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer_ptr)
            : index_mask(ring_buffer_ptr.get_buffer_size() - 1),
              index_shift(Util::log_2(ring_buffer_ptr.get_buffer_size())), ring_buffer(ring_buffer_ptr) {
            for (auto &seq: available_buffer) {
                seq.set_with_release(-1);
            }
        }

        void add_gating_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > sequences) {
            gating_sequences.set_sequences(sequences);
        }

        [[gnu::hot]] size_t next(const size_t n) {
            const size_t buffer_size = ring_buffer.get_buffer_size();

            if (n < 1 || n > buffer_size) [[unlikely]] {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const size_t current_sequence = cursor.get_and_add(n);
            const size_t next_sequence = current_sequence + n;
            const size_t wrap_point = next_sequence - buffer_size;

            // Fast path: check cached gating sequence first (avoids cross-thread atomic load)
            if (cached_gating_sequence.load(std::memory_order_relaxed) < wrap_point) [[unlikely]] {
                int wait_counter = 0;
                size_t min_sequence;
                while (wrap_point > (min_sequence = gating_sequences.get())) {
                    Util::adaptive_wait(wait_counter);
                }
                cached_gating_sequence.store(min_sequence, std::memory_order_relaxed);
            }

            return next_sequence;
        }

        [[gnu::hot]] void publish(const size_t sequence) {
            set_available(sequence);
        }

        void publish(const size_t low, const size_t high) {
            // Batch publish: plain stores for all slots, single release fence at end.
            // Consumer uses acquire loads, so release-acquire pairing guarantees visibility.
            for (size_t i = low; i <= high; ++i) {
                const size_t index = calculate_index(i);
                const size_t flag = calculate_availability_flag(i);
                available_buffer[index].set(flag); // plain store, no fence
            }
            std::atomic_thread_fence(std::memory_order_release);
        }

        void set_available(const size_t sequence) {
            const size_t index = calculate_index(sequence);
            const size_t flag = calculate_availability_flag(sequence);
            available_buffer[index].set_with_release(flag);
        }

        [[gnu::pure]] [[nodiscard]] size_t calculate_availability_flag(const size_t sequence) const {
            return sequence >> index_shift;
        }

        [[gnu::pure]] [[nodiscard]] size_t calculate_index(const size_t sequence) const {
            return sequence & index_mask;
        }

        [[gnu::hot]] [[nodiscard]] bool is_available(const size_t sequence) const {
            const size_t index = calculate_index(sequence);
            const size_t flag = calculate_availability_flag(sequence);
            return available_buffer[index].get_with_acquire() == flag;
        }

        [[nodiscard]] Sequence &get_cursor() {
            return cursor;
        }

        /**
         * Retrieve the highest sequence that has been published for the consumer to process.
         * In a multi-producer environment, it's possible that sequence 10 has already been published by producer A, while sequence 9, handled by producer B, is still being processed.
         */
        [[gnu::hot]] [[nodiscard]] size_t get_highest_published_sequence(const size_t lower_bound,
                                                            const size_t available_sequence) const {
            for (size_t sequence = lower_bound; sequence <= available_sequence; ++sequence) {
                if (!is_available(sequence)) [[unlikely]] {
                    return sequence - 1;
                }
            }
            return available_sequence;
        }
    };
}
