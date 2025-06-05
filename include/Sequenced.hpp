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
        virtual int64_t next() = 0;

        /**
         * Chờ và claim n vị trí tiếp theo để producer sử dụng.
         *
         * Claim the next n events in sequence for publishing. This is for batch event producing.
         * Using batch producing requires a little care and some math.
         *
         * Example:
         * int n = 10;
         * int64_t hi = sequencer.next(n);
         * int64_t lo = hi - (n - 1);
         * for (int64_t sequence = lo; sequence <= hi; sequence++) {
         *     // Do work.
         * }
         * sequencer.publish(lo, hi);
         *
         * @param n the number of sequences to claim
         * @return the highest claimed sequence value
         */
        virtual int64_t next(int n) = 0;

        /**
         * Attempt to claim the next event in sequence for publishing. Will return the
         * number of the slot if there is at least requiredCapacity slots available.
         *
         * @return the claimed sequence value
         * @throws InsufficientCapacityException thrown if there is no space available in the ring buffer.
         */
        virtual int64_t tryNext() = 0;

        /**
         * Attempt to claim the next n events in sequence for publishing. Will return the
         * highest numbered slot if there is at least requiredCapacity slots available.
         *
         * @param n the number of sequences to claim
         * @return the claimed sequence value
         * @throws InsufficientCapacityException thrown if there is no space available in the ring buffer.
         */
        virtual int64_t tryNext(int n) = 0;

        /**
         * Publishes a sequence. Call when the event has been filled.
         *
         * @param sequence the sequence to be published.
         */
        virtual void publish(int64_t sequence) = 0;

        /**
         * Batch publish sequences. Called when all of the events have been filled.
         *
         * @param lo first sequence number to publish
         * @param hi last sequence number to publish
         */
        virtual void publish(int64_t lo, int64_t hi) = 0;
    };

}