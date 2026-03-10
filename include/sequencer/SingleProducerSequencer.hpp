#pragma once

#include "../common/Common.hpp"
#include "../common/Util.hpp"
#include <cassert>
#include <thread>

#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {
    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class SingleProducerSequencer final {
        // manage the sequences that have been published.
        alignas(CACHE_LINE_SIZE) Sequence cursor{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};

        // Producer hot fields — kept on the SAME cache line for locality.
        // next() reads latest_claimed_sequence + cached_gating_sequence every call.
        alignas(CACHE_LINE_SIZE) size_t latest_claimed_sequence{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};
        size_t cached_gating_sequence{0};
        const char padding_1[CACHE_LINE_SIZE - sizeof(size_t) * 2] = {};
        const char padding_2[CACHE_LINE_SIZE] = {};

        const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer;
        SequenceGroupForSingleThread<NUMBER_GATING_SEQUENCES> gating_sequences;

#ifndef NDEBUG
        std::thread::id owner_thread_id_{};
        bool owner_set_{false};

        bool same_thread() {
            if (!owner_set_) {
                owner_thread_id_ = std::this_thread::get_id();
                owner_set_ = true;
                return true;
            }
            return owner_thread_id_ == std::this_thread::get_id();
        }
#endif

    public:
        explicit
        SingleProducerSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer) : ring_buffer(ring_buffer) {
        }

        void add_gating_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > sequences) {
            gating_sequences.set_sequences(sequences);
        }

        [[gnu::hot]] size_t next(const size_t n) {
            assert(same_thread() && "Accessed by two threads - use ProducerType.MULTI!");
            const size_t buffer_size = ring_buffer.get_buffer_size();

            if (n < 1 || n > buffer_size) [[unlikely]] {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const size_t next_sequence = latest_claimed_sequence + n;
            const size_t wrap_point = next_sequence - buffer_size;

            // Fast path: cached value on same cache line as latest_claimed_sequence
            if (cached_gating_sequence < wrap_point) [[unlikely]] {
                int wait_counter = 0;
                size_t min_sequence;
                while (wrap_point > (min_sequence = gating_sequences.get())) {
                    Util::adaptive_wait(wait_counter);
                }
                cached_gating_sequence = min_sequence;
            }

            latest_claimed_sequence = next_sequence;

            return next_sequence;
        }

        [[gnu::hot]] void publish(const size_t sequence) {
            cursor.set_with_release(sequence);
        }

        void publish(const size_t lo, const size_t hi) {
            publish(hi);
        }

        [[nodiscard]] bool is_available(const size_t sequence) const {
            const size_t current_sequence = cursor.get_with_acquire();
            return sequence <= current_sequence && sequence > current_sequence - ring_buffer.get_buffer_size();
        }

        [[nodiscard]] size_t get_highest_published_sequence(const size_t next_sequence,
                                                            const size_t available_sequence) const {
            return available_sequence;
        }

        [[nodiscard]] Sequence &get_cursor() {
            return cursor;
        }

    };
}
