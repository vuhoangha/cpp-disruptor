#pragma once

namespace disruptor
{
    /**
     * Coordination barrier for tracking the cursor for publishers and sequence of
     * dependent EventProcessors for processing a data structure
     */
    class SequenceBarrier
    {
    public:
        virtual ~SequenceBarrier() = default;

        /**
         * Wait for the given sequence to be available for consumption.
         *
         * @param sequence to wait for
         * @return the sequence up to which is available
         * @throws AlertException if a status change has occurred for the Disruptor
         * @throws TimeoutException if a timeout occurs while waiting for the supplied sequence.
         */
        [[nodiscard]] virtual int64_t waitFor(int64_t sequence) = 0;

        /**
         * Get the current cursor value that can be read.
         *
         * @return value of the cursor for entries that have been published.
         */
        [[nodiscard]] virtual int64_t getCursor() = 0;

        /**
         * The current alert status for the barrier.
         *
         * @return true if in alert otherwise false.
         */
        [[nodiscard]] virtual bool isAlerted() const = 0;

        /**
         * Alert the EventProcessors of a status change and stay in this status until cleared.
         */
        virtual void alert() = 0;

        /**
         * Clear the current alert status.
         */
        virtual void clearAlert() = 0;

        /**
         * Check if an alert has been raised and throw an AlertException if it has.
         *
         * @throws AlertException if alert has been raised.
         */
        virtual void checkAlert() const = 0;
    };

} // namespace disruptor