#include <gtest/gtest.h>
#include "ring_buffer/RingBuffer.hpp"
#include <string>

#include "TestEvent.hpp"

class RingBufferTest : public testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 16;
    disruptor::RingBuffer<TestEvent, BUFFER_SIZE> ringBuffer{createTestEvent};
};

TEST_F(RingBufferTest, ShouldReturnCorrectBufferSize) {
    ASSERT_EQ(BUFFER_SIZE, ringBuffer.get_buffer_size());
}

TEST_F(RingBufferTest, ShouldPrepopulateTheBufferOnConstruction) {
    for (size_t i = 0; i < BUFFER_SIZE; ++i) {
        TestEvent &event = ringBuffer.get(i);
        // Check if the event was initialized by the factory
        ASSERT_EQ(event.value, -1);
        ASSERT_EQ(event.message, "Initial");
    }
}

TEST_F(RingBufferTest, ShouldGetElementBySequence) {
    TestEvent &event = ringBuffer.get(5);
    ASSERT_NE(&event, nullptr);
}

TEST_F(RingBufferTest, ShouldGetAndModifyElement) {
    // Get the element at sequence 10
    TestEvent &event = ringBuffer.get(10);

    // Check its initial value
    ASSERT_EQ(event.value, -1);

    // Modify the element
    event.value = 12345;
    event.message = "Modified";

    // Get the element at the same sequence again
    const TestEvent &sameEvent = ringBuffer.get(10);

    // Verify that the changes are reflected, confirming we got a reference
    ASSERT_EQ(sameEvent.value, 12345);
    ASSERT_EQ(sameEvent.message, "Modified");
}

TEST_F(RingBufferTest, ShouldWrapAroundTheBuffer) {
    constexpr size_t sequence = 3;
    constexpr size_t wrappedSequence = sequence + BUFFER_SIZE * 2;

    // Modify the element at the initial sequence
    ringBuffer.get(sequence).value = 42;

    // Accessing the element at the wrapped sequence should yield the same element
    TestEvent &wrappedEvent = ringBuffer.get(wrappedSequence);
    ASSERT_EQ(wrappedEvent.value, 42);

    // Ensure they are the same object by checking their memory address
    ASSERT_EQ(&ringBuffer.get(sequence), &wrappedEvent);
}
