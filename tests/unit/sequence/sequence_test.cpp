#include <gtest/gtest.h>

#include "Sequence.hpp"

TEST(SequenceTest, InitialValueIsCorrect) {
    disruptor::Sequence sequence;
    EXPECT_EQ(sequence.get(), 0);

    disruptor::Sequence sequenceWithValue(10);
    EXPECT_EQ(sequenceWithValue.get(), 10);
}

TEST(SequenceTest, ShouldSetAndGetValue) {
    disruptor::Sequence sequence;
    sequence.set(456);
    EXPECT_EQ(456, sequence.get());
}

TEST(SequenceTest, ShouldSetAndGetRelaxedValue) {
    disruptor::Sequence sequence;
    sequence.set_relax(789);
    EXPECT_EQ(789, sequence.get_relax());
}

TEST(SequenceTest, ShouldGetAndAddRelax) {
    disruptor::Sequence sequence(10);
    size_t result = sequence.get_and_add_relax(5);
    EXPECT_EQ(10, result);
    EXPECT_EQ(15, sequence.get());
}

TEST(SequenceTest, ShouldConvertToString) {
    disruptor::Sequence sequence(987);
    EXPECT_EQ("987", sequence.to_string());
}
