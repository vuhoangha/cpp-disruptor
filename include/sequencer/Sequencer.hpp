#pragma once

#include "../sequence/Sequence.hpp"

namespace disruptor {
    class Sequencer {
    public:
        /**
         * Virtual destructor for proper cleanup in derived classes
         */
        virtual ~Sequencer() = default;

        /**
         * Claim a specific sequence. Only used if initialising the ring buffer to
         * a specific value.
         *
         * @param sequence The sequence to initialise to.
         */
        virtual void claim(size_t sequence) = 0;

        /**
         * Confirms if a sequence is published and the event is available for use; non-blocking.
         *
         * @param sequence of the buffer to check
         * @return true if the sequence is available for use, false if not
         */
        [[nodiscard]] virtual bool is_available(size_t sequence) const = 0;


        virtual void add_gating_sequences(std::initializer_list<std::reference_wrapper<Sequence> > sequences) = 0;

        /**
         * Get the highest sequence number that can be safely read from the ring buffer. Depending
         * on the implementation of the Sequencer this call may need to scan a number of values
         * in the Sequencer. The scan will range from nextSequence to availableSequence. If
         * there are no available values >= nextSequence the return value will be
         * nextSequence - 1. To work correctly a consumer should pass a value that
         * is 1 higher than the last sequence that was successfully processed.
         *
         * @param next_sequence      The sequence to start scanning from.
         * @param available_sequence The sequence to scan to.
         * @return The highest value that can be safely read, will be at least nextSequence - 1.
         */
        [[nodiscard]] virtual size_t get_highest_published_sequence(size_t next_sequence, size_t available_sequence) const = 0;

        /**
         * Claim the next event in sequence for publishing.
         *
         * @return the claimed sequence value
         */
        virtual size_t next() = 0;

        /**
         * Chờ và claim n vị trí tiếp theo để producer sử dụng.
         *
         * Claim the next n events in sequence for publishing. This is for batch event producing.
         * Using batch producing requires a little care and some math.
         *
         * Example:
         * int n = 10;
         * size_t hi = sequencer.next(n);
         * size_t lo = hi - (n - 1);
         * for (size_t sequence = lo; sequence <= hi; sequence++) {
         *     // Do work.
         * }
         * sequencer.publish(lo, hi);
         *
         * @param n the number of sequences to claim
         * @return the highest claimed sequence value
         */
        virtual size_t next(size_t n) = 0;

        /**
         * Publishes a sequence. Call when the event has been filled.
         *
         * @param sequence the sequence to be published.
         */
        virtual void publish(size_t sequence) = 0;

        /**
         * Batch publish sequences. Called when all of the events have been filled.
         *
         * @param lo first sequence number to publish
         * @param hi last sequence number to publish
         */
        virtual void publish(size_t lo, size_t hi) = 0;
    };
}
