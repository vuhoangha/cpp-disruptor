#pragma once

namespace disruptor
{

    /**
     * Operations related to the sequencing of items in a RingBuffer.
     */
    class Sequenced
    {
    public:
        /**
         * Virtual destructor for proper cleanup in derived classes
         */
        virtual ~Sequenced() = default;


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