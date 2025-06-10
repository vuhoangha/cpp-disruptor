#pragma once

#include "Sequenced.hpp"
#include "Sequence.hpp"

namespace disruptor {
    class Sequencer : public Sequenced {
    public:
        /**
         * Claim a specific sequence. Only used if initialising the ring buffer to
         * a specific value.
         *
         * @param sequence The sequence to initialise to.
         */
        virtual void claim(int64_t sequence) = 0;

        /**
         * Confirms if a sequence is published and the event is available for use; non-blocking.
         *
         * @param sequence of the buffer to check
         * @return true if the sequence is available for use, false if not
         */
        virtual bool isAvailable(int64_t sequence) const = 0;


        virtual void addGatingSequences(const std::initializer_list<std::reference_wrapper<Sequence> > sequences) = 0;

        /**
         * Get the highest sequence number that can be safely read from the ring buffer. Depending
         * on the implementation of the Sequencer this call may need to scan a number of values
         * in the Sequencer. The scan will range from nextSequence to availableSequence. If
         * there are no available values >= nextSequence the return value will be
         * nextSequence - 1. To work correctly a consumer should pass a value that
         * is 1 higher than the last sequence that was successfully processed.
         *
         * @param nextSequence      The sequence to start scanning from.
         * @param availableSequence The sequence to scan to.
         * @return The highest value that can be safely read, will be at least nextSequence - 1.
         */
        virtual int64_t getHighestPublishedSequence(const int64_t nextSequence, const int64_t availableSequence) const = 0;
    };
}
