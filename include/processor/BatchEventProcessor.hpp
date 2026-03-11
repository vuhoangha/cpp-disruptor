#pragma once

/**
 * @file BatchEventProcessor.hpp
 * @brief Consumer that processes events from the ring buffer in batches.
 *
 * This is the consumer-side workhorse. It:
 *   1. Waits for new events via the sequence barrier
 *   2. Processes all available events in a batch (not one at a time)
 *   3. Updates its sequence cursor after each batch (single release fence)
 *
 * The "batch" behavior is key to performance: instead of updating the cursor
 * after every event, it processes all available events [next, available_sequence]
 * then publishes a single cursor update. This reduces release fence overhead
 * and allows the producer to advance further before being gated.
 *
 * The event handler is called with signature:
 *   handler(T& event, size_t sequence, bool end_of_batch)
 *
 * @tparam T            Event type.
 * @tparam BUFFER_SIZE  Ring buffer slot count (power of 2).
 * @tparam EventHandler Callable with operator()(T&, size_t, bool).
 * @tparam BarrierType  ProcessingSequenceBarrier type.
 */

#include "../sequence/Sequence.hpp"
#include "../ring_buffer/RingBuffer.hpp"
#include "../common/Util.hpp"

namespace disruptor {

    template<typename T, size_t BUFFER_SIZE, typename EventHandler, typename BarrierType>
    class BatchEventProcessor final {
        Sequence sequence;
        BarrierType &sequence_barrier;
        EventHandler event_handler;
        RingBuffer<T, BUFFER_SIZE> &ring_buffer;

    public:
        explicit BatchEventProcessor(BarrierType &barrier, EventHandler handler, RingBuffer<T, BUFFER_SIZE> &ring_buffer_ptr)
            : sequence(Util::calculate_initial_value_sequence(ring_buffer_ptr.get_buffer_size())),
              sequence_barrier(barrier),
              event_handler(std::move(handler)),
              ring_buffer(ring_buffer_ptr) {}

        [[nodiscard]] Sequence &get_cursor() {
            return sequence;
        }

        /// Signal this processor to stop after the current batch completes.
        void halt() const {
            sequence_barrier.alert();
        }

        void run() {
            sequence_barrier.clear_alert();
            process_events();
        }

        /**
         * @brief Main event processing loop.
         *
         * Outer loop: wait for new events via the barrier.
         * Inner loop: process all events in [next_sequence, available_sequence].
         *
         * The adaptive_wait handles the multi-producer edge case where the cursor
         * has advanced (a sequence was claimed) but the event hasn't been published yet.
         * In this case, available_sequence < next_sequence, and we spin briefly
         * before retrying.
         */
        [[gnu::hot]] void process_events() {
            size_t next_sequence = sequence.get() + 1;
            int wait_counter = 0;

            while (true) {
                const size_t available_sequence = sequence_barrier.wait_for(next_sequence);

                if (available_sequence == SEQUENCE_ALERT) [[unlikely]] break;

                // Multi-producer gap: sequence claimed but not yet published
                if (available_sequence < next_sequence) [[unlikely]] {
                    Util::adaptive_wait(wait_counter);
                    continue;
                }

                wait_counter = 0;

                // Process all available events in one batch
                while (next_sequence <= available_sequence) {
                    // Prefetch the NEXT entry while processing the current one,
                    // hiding memory latency behind computation.
                    __builtin_prefetch(&ring_buffer.get(next_sequence + 1), 0, 3);
                    T &event = ring_buffer.get(next_sequence);
                    event_handler(event, next_sequence, next_sequence == available_sequence);
                    next_sequence++;
                }

                // Single release fence for the entire batch — much cheaper than per-event
                sequence.set_with_release(available_sequence);
            }
        }
    };

    // Deduction guide
    template<typename T, size_t BUFFER_SIZE, typename EventHandler, typename BarrierType>
    BatchEventProcessor(BarrierType &, EventHandler, RingBuffer<T, BUFFER_SIZE> &)
        -> BatchEventProcessor<T, BUFFER_SIZE, EventHandler, BarrierType>;

} // namespace disruptor
