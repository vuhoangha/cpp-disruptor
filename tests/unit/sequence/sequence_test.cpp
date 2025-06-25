#include <gtest/gtest.h>

#include "Sequence.hpp"

TEST(SequenceTest, InitialValueIsCorrect) {
    disruptor::Sequence sequence;
    EXPECT_EQ(sequence.get_with_acquire(), 0);

    disruptor::Sequence sequenceWithValue(10);
    EXPECT_EQ(sequenceWithValue.get_with_acquire(), 10);
}

TEST(SequenceTest, ShouldSetAndGetValue) {
    disruptor::Sequence sequence;
    sequence.set_with_release(456);
    EXPECT_EQ(456, sequence.get_with_acquire());
}

TEST(SequenceTest, ShouldSetAndGetRelaxedValue) {
    disruptor::Sequence sequence;
    sequence.set_relaxxxx(789);
    EXPECT_EQ(789, sequence.get_relaxxxx());
}

TEST(SequenceTest, ShouldGetAndAddRelax) {
    disruptor::Sequence sequence(10);
    size_t result = sequence.get_and_add_relaxxx(5);
    EXPECT_EQ(10, result);
    EXPECT_EQ(15, sequence.get_with_acquire());
}
