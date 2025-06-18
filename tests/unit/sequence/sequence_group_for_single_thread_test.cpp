#include <gtest/gtest.h>

#include "Sequence.hpp"
#include "SequenceGroupForSingleThread.hpp"


//----------SINGLE SEQUENCE------------------
class SequenceGroupForSingleThreadTestOne : public testing::Test {
protected:
    disruptor::Sequence s1;
    disruptor::SequenceGroupForSingleThread<1> group;

    void SetUp() override {
        s1.set(10);
        group.set_sequences({std::ref(s1)});
    }
};

TEST_F(SequenceGroupForSingleThreadTestOne, ShouldGetInitialCacheValue) {
    ASSERT_EQ(group.get_cache(), 0);
}

TEST_F(SequenceGroupForSingleThreadTestOne, ShouldGetSequenceValue) {
    ASSERT_EQ(group.get(), 10);
    ASSERT_EQ(group.get_cache(), 10);
}

TEST_F(SequenceGroupForSingleThreadTestOne, ShouldUpdateCacheWhenSequenceChanges) {
    static_cast<void>(group.get());
    s1.set(15);
    ASSERT_EQ(group.get(), 15);
    ASSERT_EQ(group.get_cache(), 15);
}


//----------MULTI SEQUENCE------------------
class SequenceGroupForSingleThreadTest : public testing::Test {
protected:
    disruptor::Sequence s1, s2, s3;
    disruptor::SequenceGroupForSingleThread<3> group;

    void SetUp() override {
        s1.set(10);
        s2.set(20);
        s3.set(5);
        group.set_sequences({std::ref(s1), std::ref(s2), std::ref(s3)});
    }
};

TEST_F(SequenceGroupForSingleThreadTest, ShouldGetInitialCacheValue) {
    ASSERT_EQ(group.get_cache(), 0);
}

TEST_F(SequenceGroupForSingleThreadTest, ShouldGetMinimumSequence) {
    ASSERT_EQ(group.get(), 5);
    ASSERT_EQ(group.get_cache(), 5);
}

TEST_F(SequenceGroupForSingleThreadTest, ShouldUpdateCacheWhenMinSequenceChanges) {
    static_cast<void>(group.get());
    s3.set(15);
    ASSERT_EQ(group.get(), 10);
    ASSERT_EQ(group.get_cache(), 10);
}

TEST_F(SequenceGroupForSingleThreadTest, ShouldHandleSameSequenceValues) {
    s1.set(5);
    static_cast<void>(group.get());
    ASSERT_EQ(group.get(), 5);
    ASSERT_EQ(group.get_cache(), 5);
}
