#pragma once

namespace disruptor
{
    /**
     * Track the required processor for the processors to process
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
         */
        [[nodiscard]] virtual size_t wait_for(size_t sequence) = 0;

        /**
         * The current alert status for the barrier.
         *
         * @return true if in alert otherwise false.
         */
        [[nodiscard]] virtual bool is_alerted() const = 0;

        /**
         * Alert the EventProcessors of a status change and stay in this status until cleared.
         */
        virtual void alert() = 0;

        /**
         * Clear the current alert status.
         */
        virtual void clear_alert() = 0;

        /**
         * Check if an alert has been raised and throw an AlertException if it has.
         *
         * @throws AlertException if alert has been raised.
         */
        virtual void check_alert() const = 0;
    };

}