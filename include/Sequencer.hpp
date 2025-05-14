#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include "Cursored.hpp"
#include "Sequenced.hpp"
#include "Sequence.hpp"
#include "SequenceBarrier.hpp"

namespace disruptor
{
    class Sequencer : public Cursored, public Sequenced
    {
    public:
        static constexpr int64_t INITIAL_CURSOR_VALUE = Sequence::INITIAL_VALUE;

        virtual ~Sequencer() = default;

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

        /**
         * Add the specified gating sequences to this instance of the Disruptor. They will
         * safely and atomically added to the list of gating sequences.
         *
         * @param gatingSequences The sequences to add.
         */
        virtual void addGatingSequences(const std::vector<std::shared_ptr<Sequence>> &gatingSequences) = 0;

        /**
         * Remove the specified sequence from this sequencer.
         *
         * @param sequence to be removed.
         * @return true if this sequence was found, false otherwise.
         */
        virtual bool removeGatingSequence(const std::shared_ptr<Sequence> &sequence) = 0;

        /**
         * Create a new SequenceBarrier to be used by an EventProcessor to track which messages
         * are available to be read from the ring buffer given a list of sequences to track.
         *
         * @param sequencesToTrack All of the sequences that the newly constructed barrier will wait on.
         * @return A sequence barrier that will track the specified sequences.
         * @see SequenceBarrier
         */
        virtual std::shared_ptr<SequenceBarrier> newBarrier(
            const std::vector<std::shared_ptr<Sequence>> &sequencesToTrack) = 0;

        /**
         * Get the minimum sequence value from all of the gating sequences
         * added to this ringBuffer.
         *
         * @return The minimum gating sequence or the cursor sequence if
         * no sequences have been added.
         */
        virtual int64_t getMinimumSequence() const = 0;

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
        virtual int64_t getHighestPublishedSequence(int64_t nextSequence, int64_t availableSequence) const = 0;
    };
}
