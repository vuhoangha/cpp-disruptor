#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <thread>
#include "ProcessingSequenceBarrier.hpp"
#include "Sequencer.hpp"
#include "Sequence.hpp"
#include "WaitStrategyType.hpp"
#include <future>
#include <chrono>

using namespace testing;
using namespace disruptor;

// Mock Sequencer cho việc kiểm thử
class MockSequencer final : public Sequencer {
public:
    MOCK_METHOD(size_t, next, (size_t n), (override));
    MOCK_METHOD(void, publish, (size_t sequence), (override));
    MOCK_METHOD(void, publish, (size_t lo, size_t hi), (override));
    MOCK_METHOD(bool, is_available, (size_t sequence), (const, override));
    MOCK_METHOD(size_t, get_highest_published_sequence, (size_t lo_bound, size_t hi_bound), (const, override));
    MOCK_METHOD(void, add_gating_sequences, (std::initializer_list<std::reference_wrapper<disruptor::Sequence>>), (override));
};

class ProcessingSequenceBarrierTest : public Test {
protected:
    void SetUp() override {
        sequencer = std::make_unique<MockSequencer>();
        sequencer_cursor.set_with_release(0); // Khởi tạo cursor của sequencer
    }

    std::unique_ptr<MockSequencer> sequencer;
    disruptor::Sequence sequencer_cursor; // Cursor của sequencer
    disruptor::Sequence processor1_cursor; // Cursor của processor1
    disruptor::Sequence processor2_cursor; // Cursor của processor2
};

// Test với direct_publisher_event_listener = true và một sequence duy nhất (cursor của sequencer)
TEST_F(ProcessingSequenceBarrierTest, DirectListenerWithSingleSequence) {
    ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1> barrier(
        true, {std::ref(sequencer_cursor)}, *sequencer);

    sequencer_cursor.set_with_release(10);

    EXPECT_CALL(*sequencer, get_highest_published_sequence(10, 10))
            .WillOnce(Return(10));

    const size_t result = barrier.wait_for(10);
    EXPECT_EQ(result, 10);
}

// Test với direct_publisher_event_listener = false và nhiều sequence (cursor của processor)
TEST_F(ProcessingSequenceBarrierTest, IndirectListenerWithMultipleSequences) {
    processor1_cursor.set_with_release(8);
    processor2_cursor.set_with_release(7);

    ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 2> barrier(
        false, {std::ref(processor1_cursor), std::ref(processor2_cursor)}, *sequencer);

    std::thread update_thread([this]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        this->processor1_cursor.set_with_release(12);
        this->processor2_cursor.set_with_release(11);
    });

    const size_t result = barrier.wait_for(10);

    update_thread.join();

    EXPECT_EQ(result, 11);
}

// Test với direct_publisher_event_listener = true
TEST_F(ProcessingSequenceBarrierTest, DirectListenerWithSequencerUpdate) {
    // Thiết lập giá trị ban đầu cho sequencer_cursor
    sequencer_cursor.set_with_release(5);

    // Khởi tạo barrier với direct_publisher_event_listener = true
    ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1> barrier(
        true, {std::ref(sequencer_cursor)}, *sequencer);

    // Thiết lập kỳ vọng cho get_highest_published_sequence
    EXPECT_CALL(*sequencer, get_highest_published_sequence(10, 15))
            .WillOnce(Return(10));

    // Tạo thread cập nhật sequencer_cursor sau 200ms
    std::thread update_thread([this]() {
        // Chờ 200ms
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Cập nhật sequencer_cursor lên 15
        this->sequencer_cursor.set_with_release(15);
    });

    // Gọi wait_for từ thread chính, yêu cầu sequence 10
    size_t result = barrier.wait_for(10);

    // Đảm bảo thread cập nhật đã hoàn thành
    update_thread.join();

    // Kết quả phải là giá trị trả về từ get_highest_published_sequence
    EXPECT_EQ(result, 10);
}

// Test alert trong khi đang wait
TEST_F(ProcessingSequenceBarrierTest, AlertDuringWait) {
    // Thiết lập giá trị ban đầu cho processor cursor
    processor1_cursor.set_with_release(5);

    // Khởi tạo barrier
    ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1> barrier(
        false, {std::ref(processor1_cursor)}, *sequencer);

    // Tạo thread gọi alert() sau 200ms
    std::thread alert_thread([&barrier]() {
        // Chờ 200ms
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Đặt alert
        barrier.alert();
    });

    // Gọi wait_for từ thread chính, yêu cầu sequence cao hơn (10)
    // Phải ném AlertException khi alert được đặt
    EXPECT_THROW(barrier.wait_for(10), AlertException);

    // Đảm bảo thread alert đã hoàn thành
    alert_thread.join();
}

// Test với nhiều loại wait strategy khác nhau
TEST_F(ProcessingSequenceBarrierTest, DifferentWaitStrategies) {
    // Test với AdaptiveWaitStrategy
    {
        processor1_cursor.set_with_release(5);
        ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 1> barrier(
            false, {std::ref(processor1_cursor)}, *sequencer);

        std::thread update_thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            this->processor1_cursor.set_with_release(15);
        });

        const size_t result = barrier.wait_for(10);
        update_thread.join();
        EXPECT_EQ(result, 15);
    }

    // Test với YieldingWaitStrategy
    {
        processor1_cursor.set_with_release(5);
        ProcessingSequenceBarrier<WaitStrategyType::YIELD, 1> barrier(
            false, {std::ref(processor1_cursor)}, *sequencer);

        std::thread update_thread([this]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            this->processor1_cursor.set_with_release(16);
        });

        const size_t result = barrier.wait_for(10);
        update_thread.join();
        EXPECT_EQ(result, 16);
    }
}

// Test trường hợp nhiều dependent processor cập nhật riêng biệt
TEST_F(ProcessingSequenceBarrierTest, MultipleProcessorUpdates) {
    // Thiết lập giá trị ban đầu
    processor1_cursor.set_with_release(50);
    processor2_cursor.set_with_release(50);

    // Khởi tạo barrier
    ProcessingSequenceBarrier<WaitStrategyType::ADAPTIVE, 2> barrier(
        false, {std::ref(processor1_cursor), std::ref(processor2_cursor)}, *sequencer);

    // Tạo thread cập nhật các processor theo kiểu xen kẽ
    std::thread update_thread([this]() {
        // Cập nhật processor1 trước
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        this->processor1_cursor.set_with_release(120);

        // Sau đó cập nhật processor2
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        this->processor2_cursor.set_with_release(100);
    });

    // Gọi wait_for từ thread chính, yêu cầu sequence 10
    const size_t result = barrier.wait_for(100);

    // Đảm bảo thread cập nhật đã hoàn thành
    update_thread.join();

    // Kết quả phải là giá trị nhỏ nhất trong các dependent sequence
    EXPECT_EQ(result, 100); // min(12, 10) = 10
}
