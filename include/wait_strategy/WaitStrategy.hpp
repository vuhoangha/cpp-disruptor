#pragma once

#include "../barriers/SequenceBarrier.hpp"
#include "../sequence/SequenceGroupForSingleThread.hpp"

namespace disruptor {
    /**
     * Strategy employed for making EventProcessors wait on a cursor Sequence.
     */
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class WaitStrategy {
    public:
        virtual ~WaitStrategy() = default;

        /**
         * Wait for the given sequence to be available. It is possible for this method to return a value
         * less than the sequence number supplied depending on the implementation of the WaitStrategy. A common
         * use for this is to signal a timeout. Any EventProcessor that is using a WaitStrategy to get notifications
         * about message becoming available should remember to handle this case. The BatchEventProcessor explicitly
         * handles this case and will signal a timeout if required.
         *
         * @param sequence          to be waited on.
         * @param dependent_sequences            the main or dependent sequences
         * @param barrier           the processor is waiting on.
         * @return the sequence that is available which may be greater than the requested sequence.
         * @throws AlertException if the status of the Disruptor has changed.
         * @throws TimeoutException if a timeout occurs before waiting completes (not used by some strategies)
         */
        [[nodiscard]] virtual size_t wait_for(
            size_t sequence,
            SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
            const SequenceBarrier &barrier) = 0;

        /**
         * String representation of the wait strategy
         */
        [[nodiscard]] virtual std::string to_string() const = 0;
    };
} // namespace disruptor
