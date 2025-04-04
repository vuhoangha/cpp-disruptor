#pragma once

#include <cstdint>
#include "Sequence.h"
#include "SequenceBarrier.h"
#include "AlertException.h"
#include "TimeoutException.h"

namespace disruptor
{

    /**
     * Strategy employed for making EventProcessors wait on a cursor Sequence.
     */
    class WaitStrategy
    {
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
         * @param cursor            the main sequence from ringbuffer. Wait/notify strategies will
         *                         need this as it's the only sequence that is also notified upon update.
         * @param dependentSequence on which to wait.
         * @param barrier           the processor is waiting on.
         * @return the sequence that is available which may be greater than the requested sequence.
         * @throws AlertException if the status of the Disruptor has changed.
         * @throws TimeoutException if a timeout occurs before waiting completes (not used by some strategies)
         */
        [[nodiscard]] virtual int64_t waitFor(
            int64_t sequence,
            const Sequence &cursor,
            const Sequence &dependentSequence,
            const SequenceBarrier &barrier) = 0;

        /**
         * Implementations should signal the waiting EventProcessors that the cursor has advanced.
         */
        virtual void signalAllWhenBlocking() noexcept = 0;
    };

} // namespace disruptor