#pragma once

#include <cstddef>

namespace disruptor {

    /**
     * BatchProducer — transparent batch claiming wrapper for multi-producer scenarios.
     *
     * Reduces atomic contention by claiming BATCH_SIZE slots at once from the sequencer,
     * then handing them out one at a time. From the user's perspective, the API looks
     * identical to single-event publishing, but internally it batches.
     *
     * Performance impact (2P-1C benchmark, i9-14900K):
     *   batch=1:  ~24M ops/s  (baseline)
     *   batch=32: ~645M ops/s (27x improvement)
     *
     * Usage:
     *   BatchProducer<decltype(sequencer), decltype(ring_buffer), 32> producer(sequencer, ring_buffer);
     *
     *   for (...) {
     *       auto& event = producer.next();
     *       event.value = ...;
     *       producer.publish();
     *   }
     *   producer.flush();  // IMPORTANT: flush remaining events at the end
     *
     * @tparam SequencerType  MultiProducerSequencer type
     * @tparam RingBufferType RingBuffer type
     * @tparam BATCH_SIZE     Number of slots to claim at once (default 32, must be power of 2)
     */
    template<typename SequencerType, typename RingBufferType, size_t BATCH_SIZE = 32>
    class BatchProducer final {
        static_assert(BATCH_SIZE > 0 && (BATCH_SIZE & (BATCH_SIZE - 1)) == 0,
                      "BATCH_SIZE must be a power of 2");

        SequencerType& sequencer;
        RingBufferType& ring_buffer;
        size_t batch_lo = 0;     // first sequence in current batch
        size_t batch_hi = 0;     // last sequence in current batch
        size_t current = 0;      // next sequence to hand out
        bool has_batch = false;  // whether we have an active claimed batch

        void claim_batch() {
            batch_hi = sequencer.next(BATCH_SIZE);
            batch_lo = batch_hi - BATCH_SIZE + 1;
            current = batch_lo;
            has_batch = true;
        }

    public:
        BatchProducer(SequencerType& seq, RingBufferType& rb)
            : sequencer(seq), ring_buffer(rb) {}

        // Non-copyable, non-movable (holds references)
        BatchProducer(const BatchProducer&) = delete;
        BatchProducer& operator=(const BatchProducer&) = delete;

        /**
         * Claim the next event slot. Internally batched — only touches the atomic cursor
         * once per BATCH_SIZE calls.
         *
         * @return Reference to the ring buffer event to fill in
         */
        [[gnu::hot]] auto& next() {
            if (!has_batch || current > batch_hi) [[unlikely]] {
                claim_batch();
            }
            return ring_buffer.get(current);
        }

        /**
         * Publish the current event. When the batch is full, publishes the entire
         * batch with a single release fence (much cheaper than per-event fencing).
         */
        [[gnu::hot]] void publish() {
            if (current == batch_hi) [[unlikely]] {
                // Last slot in batch — publish all at once
                sequencer.publish(batch_lo, batch_hi);
                has_batch = false;
            }
            current++;
        }

        /**
         * Flush any remaining unpublished events in the current batch.
         * MUST be called when the producer is done, otherwise the last partial
         * batch will never be visible to consumers.
         */
        void flush() {
            if (has_batch && current > batch_lo) {
                sequencer.publish(batch_lo, current - 1);
                has_batch = false;
            }
        }

        /**
         * Returns the current batch size configuration.
         */
        [[nodiscard]] static constexpr size_t batch_size() { return BATCH_SIZE; }
    };

} // namespace disruptor
