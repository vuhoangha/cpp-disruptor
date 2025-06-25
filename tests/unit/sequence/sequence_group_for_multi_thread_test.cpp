#include <gtest/gtest.h>
#include "Sequence.hpp"
#include "SequenceGroupForMultiThread.hpp"

//----------SINGLE SEQUENCE------------------
TEST(SequenceGroupForMultiThreadTest, ShouldGetSingleSequenceValue) {
    disruptor::Sequence s1(42);
    const disruptor::SequenceGroupForMultiThread<1> group{std::ref(s1)};

    EXPECT_EQ(42, group.get());

    s1.set_with_release(50);
    EXPECT_EQ(50, group.get());
}

TEST(SequenceGroupForMultiThreadTest, ShouldHandleInitialEmptyGroupForSingleSequence) {
    disruptor::SequenceGroupForMultiThread<1> group;
    disruptor::Sequence s1(51);

    group.set_sequences({std::ref(s1)});
    EXPECT_EQ(51, group.get());
}


//----------MULTI SEQUENCE------------------
TEST(SequenceGroupForMultiThreadTest, ShouldGetMinimumSequenceValue) {
    disruptor::Sequence s1(10);
    disruptor::Sequence s2(20);
    disruptor::Sequence s3(30);
    disruptor::SequenceGroupForMultiThread<3> group{std::ref(s1), std::ref(s2), std::ref(s3)};

    EXPECT_EQ(10, group.get());
}

TEST(SequenceGroupForMultiThreadTest, ShouldHandleInitialEmptyGroupForMultiSequence) {
    disruptor::SequenceGroupForMultiThread<2> group;
    disruptor::Sequence s1(5);
    disruptor::Sequence s2(8);

    group.set_sequences({std::ref(s1), std::ref(s2)});
    EXPECT_EQ(5, group.get());
}

TEST(SequenceGroupForMultiThreadTest, ShouldNotGoBackwardsWhenSequenceValueDecreases) {
    disruptor::Sequence s1(10);
    disruptor::Sequence s2(20);
    disruptor::SequenceGroupForMultiThread<2> group{std::ref(s1), std::ref(s2)};

    EXPECT_EQ(10, group.get());

    s2.set_with_release(5);
    EXPECT_EQ(5, group.get());
}

TEST(SequenceGroupForMultiThreadTest, ShouldUpdateToNewMinimumWhenAllSequencesAdvance) {
    disruptor::Sequence s1(10);
    disruptor::Sequence s2(12);
    disruptor::SequenceGroupForMultiThread<2> group{std::ref(s1), std::ref(s2)};

    EXPECT_EQ(10, group.get());

    s1.set_with_release(15);
    s2.set_with_release(18);

    EXPECT_EQ(15, group.get());
}
