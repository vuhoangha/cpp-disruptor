#pragma once

/**
 * @file SingleProducerSequencer.hpp
 * @brief Sequencer optimized for a single producer thread — no atomics on the hot path.
 *
 * Since only one thread calls next()/publish(), the cursor can be updated with a
 * simple release fence (no CAS or LOCK XADD needed). This makes single-producer
 * scenarios significantly faster than multi-producer (~800M+ ops/s vs ~25M baseline).
 *
 * Memory layout:
 *   Cache line 1: cursor (Sequence, 192 bytes with padding)
 *   Cache line 2: latest_claimed_sequence + cached_gating_sequence (same line for locality)
 *   Cold data:    ring_buffer ref, gating_sequences
 *
 * The key optimization is keeping latest_claimed_sequence and cached_gating_sequence
 * on the same cache line — next() reads both every call, so co-location avoids
 * an extra cache miss.
 *
 * @tparam T                        Event type in the ring buffer.
 * @tparam RING_BUFFER_SIZE         Number of slots (power of 2).
 * @tparam NUMBER_GATING_SEQUENCES  Number of downstream consumer sequences to track.
 */

#include "../common/Common.hpp"
#include "../common/Util.hpp"
#include <cassert>
#include <thread>

#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {

    template<typename T, size_t RING_BUFFER_SIZE, size_t NUMBER_GATING_SEQUENCES>
    class SingleProducerSequencer final {
        /// Published cursor — consumers read this (with acquire) to see new events.
        alignas(CACHE_LINE_SIZE) Sequence cursor{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};

        /// Producer-private hot fields — on the SAME cache line for locality.
        /// next() reads both every call; co-location saves one cache miss.
        alignas(CACHE_LINE_SIZE) size_t latest_claimed_sequence{Util::calculate_initial_value_sequence(RING_BUFFER_SIZE)};
        size_t cached_gating_sequence{0};  ///< Cached minimum consumer sequence (avoids cross-thread read)
        const char padding_1[CACHE_LINE_SIZE - sizeof(size_t) * 2] = {};
        const char padding_2[CACHE_LINE_SIZE] = {};

        const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer;
        SequenceGroupForSingleThread<NUMBER_GATING_SEQUENCES> gating_sequences;

#ifndef NDEBUG
        /// Debug-only: detect accidental multi-threaded access.
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
        SingleProducerSequencer(const RingBuffer<T, RING_BUFFER_SIZE> &ring_buffer) : ring_buffer(ring_buffer) {}

        void add_gating_sequences(const std::initializer_list<std::reference_wrapper<Sequence> > sequences) {
            gating_sequences.set_sequences(sequences);
        }

        /**
         * @brief Claim the next n sequence slots for publishing.
         *
         * Fast path: if cached_gating_sequence shows enough room, just increment
         * and return — no atomic operations, no cross-thread reads.
         *
         * Slow path (buffer full): spin-wait until the slowest consumer advances
         * far enough, then update the cache and proceed.
         *
         * @param n Number of slots to claim (usually 1).
         * @return The highest claimed sequence number.
         */
        [[gnu::hot]] size_t next(const size_t n) {
            assert(same_thread() && "Accessed by two threads - use ProducerType.MULTI!");
            const size_t buffer_size = ring_buffer.get_buffer_size();

            if (n < 1 || n > buffer_size) [[unlikely]] {
                throw std::invalid_argument("n must be > 0 and < bufferSize");
            }

            const size_t next_sequence = latest_claimed_sequence + n;
            const size_t wrap_point = next_sequence - buffer_size;

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

        /// Make the event at `sequence` visible to consumers (release fence + cursor update).
        [[gnu::hot]] void publish(const size_t sequence) {
            cursor.set_with_release(sequence);
        }

        /// Range publish — for single producer, just publish the highest sequence.
        void publish(const size_t lo, const size_t hi) {
            publish(hi);
        }

        [[nodiscard]] bool is_available(const size_t sequence) const {
            const size_t current_sequence = cursor.get_with_acquire();
            return sequence <= current_sequence && sequence > current_sequence - ring_buffer.get_buffer_size();
        }

        /// Single producer: all sequences up to available_sequence are guaranteed published.
        /// No gaps possible, so just return available_sequence directly.
        [[nodiscard]] size_t get_highest_published_sequence(const size_t next_sequence,
                                                            const size_t available_sequence) const {
            return available_sequence;
        }

        [[nodiscard]] Sequence &get_cursor() {
            return cursor;
        }
    };

} // namespace disruptor
