#include <gtest/gtest.h>

#include "MultiProducerSequencer.hpp"
#include "RingBuffer.hpp"
#include "TestEvent.hpp"

class MultiProducerSequencerTest : public testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 32;

    disruptor::RingBuffer<TestEvent, BUFFER_SIZE> ringBuffer{createTestEvent};
    disruptor::MultiProducerSequencer<TestEvent, BUFFER_SIZE, 1> sequencer{ringBuffer};
    disruptor::Sequence gatingSequence;

    void SetUp() override {
        gatingSequence.set_with_release(disruptor::Util::calculate_initial_value_sequence(BUFFER_SIZE));
        sequencer.add_gating_sequences({std::ref(gatingSequence)});
    }
};

TEST_F(MultiProducerSequencerTest, ShouldStartWithInitialValue) {
    const size_t initialValue = disruptor::Util::calculate_initial_value_sequence(BUFFER_SIZE);
    EXPECT_EQ(sequencer.get_cursor().get_with_acquire(), initialValue);
}

TEST_F(MultiProducerSequencerTest, ShouldClaimNextSequence) {
    const size_t initialValue = disruptor::Util::calculate_initial_value_sequence(BUFFER_SIZE);
    EXPECT_EQ(sequencer.next(1), initialValue + 1);
    EXPECT_EQ(sequencer.get_cursor().get_with_acquire(), initialValue + 1);
}

TEST_F(MultiProducerSequencerTest, ShouldClaimBatchOfSequences) {
    const size_t initialValue = disruptor::Util::calculate_initial_value_sequence(BUFFER_SIZE);
    constexpr size_t batchSize = 5;
    EXPECT_EQ(sequencer.next(batchSize), initialValue + batchSize);
    EXPECT_EQ(sequencer.get_cursor().get_with_acquire(), initialValue + batchSize);
}

TEST_F(MultiProducerSequencerTest, ShouldThrowOnInvalidBatchSize) {
    ASSERT_THROW(sequencer.next(0), std::invalid_argument);
    ASSERT_THROW(sequencer.next(BUFFER_SIZE + 1), std::invalid_argument);
}

TEST_F(MultiProducerSequencerTest, ShouldPublishAndMakeSequenceAvailable) {
    const size_t sequence = sequencer.next(1);

    ASSERT_FALSE(sequencer.is_available(sequence)); // Chưa publish

    sequencer.publish(sequence);

    ASSERT_TRUE(sequencer.is_available(sequence)); // Đã publish
    ASSERT_FALSE(sequencer.is_available(sequence + 1)); // Sequence tiếp theo chưa có
}

TEST_F(MultiProducerSequencerTest, ShouldPublishBatchAndMakeRangeAvailable) {
    constexpr size_t batchSize = 4;
    const size_t high = sequencer.next(batchSize);
    const size_t low = high - batchSize + 1;

    sequencer.publish(low, high);

    for (size_t i = low; i <= high; ++i) {
        ASSERT_TRUE(sequencer.is_available(i));
    }
    ASSERT_FALSE(sequencer.is_available(high + 1));
}

TEST_F(MultiProducerSequencerTest, ShouldBlockWhenGatingSequenceIsBehind) {
    sequencer.next(BUFFER_SIZE);

    std::atomic<bool> producer_was_blocked = true;

    std::thread consumer_thread([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        ASSERT_TRUE(producer_was_blocked.load());

        // consumer process an event
        producer_was_blocked = false;
        gatingSequence.set_with_release(sequencer.get_cursor().get_with_acquire() - BUFFER_SIZE + 1);
    });

    // wait space in ring buffer
    sequencer.next(1);

    consumer_thread.join();
    ASSERT_FALSE(producer_was_blocked.load());
}

TEST_F(MultiProducerSequencerTest, ShouldGetHighestPublishedSequenceWithGap) {
    // Claim 5 sequence
    const size_t seq5 = sequencer.next(5);
    const size_t seq4 = seq5 - 1;
    const size_t seq3 = seq5 - 2;
    const size_t seq2 = seq5 - 3;
    const size_t seq1 = seq5 - 4;

    // Publishing is out of order, creating a 'gap' at sequence 3.
    sequencer.publish(seq1);
    sequencer.publish(seq2);
    // Skip sequence 3
    sequencer.publish(seq4);
    sequencer.publish(seq5);

    // Although sequences 4 and 5 have been published, since sequence 3 is missing, the highest contiguous sequence is 2
    EXPECT_TRUE(sequencer.is_available(seq1));
    EXPECT_TRUE(sequencer.is_available(seq2));
    EXPECT_FALSE(sequencer.is_available(seq3));
    EXPECT_TRUE(sequencer.is_available(seq4));

    EXPECT_EQ(sequencer.get_highest_published_sequence(seq1, seq5), seq2);

    // Now publish the remaining gap
    sequencer.publish(seq3);

    // Now all sequences are contiguous
    EXPECT_EQ(sequencer.get_highest_published_sequence(seq1, seq5), seq5);
}

TEST_F(MultiProducerSequencerTest, ShouldProvideUniqueSequencesWithMultipleThreads) {
    constexpr int num_threads = 4;
    // Ensure that the buffer is overwritten at least once to test wrap-around behavior
    constexpr int iterations_per_thread = BUFFER_SIZE * 3;
    constexpr int total_claims = num_threads * iterations_per_thread;
    const size_t initialValue = disruptor::Util::calculate_initial_value_sequence(BUFFER_SIZE);

    std::vector<std::thread> producers;
    std::vector<std::vector<size_t> > claimed_sequences_per_thread(num_threads);

    // Consumer thread to prevent producers from blocking
    std::thread consumer_thread([&] {
        const size_t final_sequence = initialValue + total_claims;
        size_t next_sequence_to_process = initialValue + 1;
        while (next_sequence_to_process <= final_sequence) {
            // yield-wait for the sequence to be published and thus available for consumption
            while (!sequencer.is_available(next_sequence_to_process)) {
                std::this_thread::yield(); // Yield to other threads
            }
            // Move the gating sequence forward, signaling that the slot can be reclaimed
            gatingSequence.set_with_release(next_sequence_to_process);
            next_sequence_to_process++;
        }
    });

    for (int i = 0; i < num_threads; ++i) {
        producers.emplace_back([&, i] {
            claimed_sequences_per_thread[i].reserve(iterations_per_thread);
            for (int j = 0; j < iterations_per_thread; ++j) {
                size_t claimed_sequence = sequencer.next(1);
                claimed_sequences_per_thread[i].push_back(claimed_sequence);
                sequencer.publish(claimed_sequence);
            }
        });
    }

    for (auto &t: producers) {
        t.join();
    }
    consumer_thread.join();

    // --- START CHECKING ---

    // A. Verify uniqueness and the position of the cursor
    std::set<size_t> all_claimed_sequences;
    for (const auto &thread_sequences: claimed_sequences_per_thread) {
        ASSERT_EQ(thread_sequences.size(), iterations_per_thread);
        all_claimed_sequences.insert(thread_sequences.begin(), thread_sequences.end());
    }

    ASSERT_EQ(all_claimed_sequences.size(), total_claims);
    EXPECT_EQ(sequencer.get_cursor().get_with_acquire(), initialValue + total_claims);

    // B. check availability status after publishing
    const size_t highest_sequence = initialValue + total_claims;

    // All claimed and published sequences must now be available
    // and there must be no gaps
    ASSERT_EQ(sequencer.get_highest_published_sequence(highest_sequence - BUFFER_SIZE + 1, highest_sequence), highest_sequence);

    // C. wrap-around checking
    // and old sequence (whose slot has been overwritten) should no longer be available

    // Retrieve a sequence located exactly one full buffer rotation behind the highest sequence
    // It uses the same slot but with a different availability flag
    const size_t wrapped_sequence = highest_sequence - BUFFER_SIZE;
    ASSERT_FALSE(sequencer.is_available(wrapped_sequence));

    // Check the sequence immediately before it to be sure
    const size_t wrapped_sequence_minus_one = highest_sequence - BUFFER_SIZE - 1;
    ASSERT_FALSE(sequencer.is_available(wrapped_sequence_minus_one));

    // Check the sequence immediately after it to be sur
    const size_t wrapped_sequence_plus_one = highest_sequence - BUFFER_SIZE + 1;
    ASSERT_TRUE(sequencer.is_available(wrapped_sequence_plus_one));

    ASSERT_EQ(gatingSequence.get_with_acquire(), highest_sequence);
}
