#include <gtest/gtest.h>
#include <thread>
#include <chrono>

#include "RingBuffer.hpp"
#include "SingleProducerSequencer.hpp"
#include "TestEvent.hpp"

class SingleProducerSequencerTest : public testing::Test {
protected:
    static constexpr size_t BUFFER_SIZE = 16;

    // Use a pointer for the ring buffer to control its lifecycle if needed
    disruptor::RingBuffer<TestEvent, BUFFER_SIZE> ringBuffer{createTestEvent};
    // The sequencer under test
    disruptor::SingleProducerSequencer<TestEvent, BUFFER_SIZE, 1> sequencer{ringBuffer};
    // A gating sequence to simulate a consumer
    disruptor::Sequence gatingSequence;

    void SetUp() override {
        gatingSequence.set(disruptor::Util::calculate_initial_value_sequence(BUFFER_SIZE));
        sequencer.add_gating_sequences({std::ref(gatingSequence)});
    }
};


TEST_F(SingleProducerSequencerTest, ShouldStartWithInitialValue) {
    const size_t initialValue = disruptor::Util::calculate_initial_value_sequence(BUFFER_SIZE);
    EXPECT_EQ(sequencer.get_cursor().get(), initialValue);
}


TEST_F(SingleProducerSequencerTest, ShouldClaimNextSequence) {
    const size_t initialValue = disruptor::Util::calculate_initial_value_sequence(BUFFER_SIZE);
    EXPECT_EQ(sequencer.next(), initialValue + 1);
    EXPECT_EQ(sequencer.get_cursor().get(), initialValue); // Cursor not updated until publish
}


TEST_F(SingleProducerSequencerTest, ShouldClaimBatchOfSequences) {
    const size_t initialValue = disruptor::Util::calculate_initial_value_sequence(BUFFER_SIZE);
    constexpr size_t batchSize = 5;
    EXPECT_EQ(sequencer.next(batchSize), initialValue + batchSize);
    EXPECT_EQ(sequencer.get_cursor().get(), initialValue);
}


TEST_F(SingleProducerSequencerTest, ShouldThrowOnInvalidBatchSize) {
    ASSERT_THROW(sequencer.next(0), std::invalid_argument);
    ASSERT_THROW(sequencer.next(BUFFER_SIZE + 1), std::invalid_argument);
}


TEST_F(SingleProducerSequencerTest, ShouldPublishSequenceAndMoveCursor) {
    const size_t sequence = sequencer.next();
    sequencer.publish(sequence);
    EXPECT_EQ(sequencer.get_cursor().get(), sequence);
}


TEST_F(SingleProducerSequencerTest, ShouldPublishBatchAndMoveCursor) {
    constexpr size_t batchSize = 4;
    const size_t high = sequencer.next(batchSize);
    const size_t low = high - batchSize + 1;
    sequencer.publish(low, high);
    EXPECT_EQ(sequencer.get_cursor().get(), high);
}


TEST_F(SingleProducerSequencerTest, ShouldCheckAvailability) {
    const size_t sequence = sequencer.next();
    EXPECT_FALSE(sequencer.is_available(sequence));
    sequencer.publish(sequence);
    EXPECT_TRUE(sequencer.is_available(sequence));
    // Check for an old, wrapped-around sequence
    EXPECT_FALSE(sequencer.is_available(sequence - BUFFER_SIZE));
}


TEST_F(SingleProducerSequencerTest, ShouldBlockWhenGatingSequenceIsBehind) {
    // 1. Lấp đầy Ring Buffer từ thread producer chính
    const size_t nextSequence = sequencer.next(BUFFER_SIZE);
    sequencer.publish(nextSequence - BUFFER_SIZE + 1, nextSequence);

    std::atomic<bool> producer_was_blocked = true;

    // 2. Tạo một thread "consumer" để giải cứu sau một thời gian
    std::thread consumer_thread([&] {
        // Chờ một chút để đảm bảo producer đã bị khóa
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        // Kiểm tra xem producer có thực sự đang bị khóa không
        // Nếu producer_was_blocked vẫn là true, nghĩa là nó chưa chạy qua được lệnh next()
        ASSERT_TRUE(producer_was_blocked.load());

        producer_was_blocked = false;

        // Giả lập consumer đã xử lý xong và di chuyển gating sequence để giải phóng buffer
        gatingSequence.set(nextSequence - BUFFER_SIZE + 1);
    });

    // 3. Lệnh gọi next() này sẽ bị khóa lại vì buffer đã đầy
    // Nó sẽ chỉ được giải phóng khi consumer_thread ở trên cập nhật gatingSequence
    sequencer.next();

    consumer_thread.join();
    // Xác nhận lại lần cuối rằng producer đã từng bị khóa và sau đó được giải phóng
    ASSERT_FALSE(producer_was_blocked.load());
}


TEST_F(SingleProducerSequencerTest, ShouldFailWhenAccessedByMultipleThreads) {
    // Biểu thức chính gây ra lỗi assertion
    auto multi_threaded_access = [&]() {
        // Thread chính (producer 1) truy cập trước để đăng ký thread ID của nó
        sequencer.next();

        // Một thread khác (producer 2) cố gắng truy cập
        std::thread second_producer_thread([&] {
            // Lệnh gọi này sẽ vi phạm quy tắc single producer và gây ra assertion failed
            sequencer.next();
        });

        second_producer_thread.join();
    };

    // EXPECT_DEATH chạy đoạn code trong một process riêng và kiểm tra xem nó có
    // chết với thông báo lỗi mong muốn không.
    EXPECT_DEATH(multi_threaded_access(), "Accessed by two threads - use ProducerType.MULTI!");
}
