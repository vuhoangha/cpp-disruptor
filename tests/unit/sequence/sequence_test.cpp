#include <gtest/gtest.h>

#include "Sequence.hpp"

TEST(SequenceTest, InitialValueIsCorrect) {
    disruptor::Sequence sequence;
    EXPECT_EQ(sequence.get(), 0);
    
    disruptor::Sequence sequenceWithValue(10);
    EXPECT_EQ(sequenceWithValue.get(), 10);
}