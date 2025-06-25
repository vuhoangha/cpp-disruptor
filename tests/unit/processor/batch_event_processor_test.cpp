#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>
#include <atomic>
#include "BatchEventProcessor.hpp"
#include "Sequence.hpp"
#include "SequenceBarrier.hpp"
#include "RingBuffer.hpp"
#include "AlertException.hpp"
#include "TestEvent.hpp"

using namespace testing;
using namespace disruptor;

// Mock cho SequenceBarrier
class MockSequenceBarrier final : public SequenceBarrier {
public:
    MOCK_METHOD(size_t, wait_for, (size_t sequence), (override));
    MOCK_METHOD(bool, is_alerted, (), (const, override));
    MOCK_METHOD(void, alert, (), (override));
    MOCK_METHOD(void, clear_alert, (), (override));
    MOCK_METHOD(void, check_alert, (), (const, override));
};

class BatchEventProcessorTest : public Test {
protected:
    void SetUp() override {
        // Khởi tạo mock sequence barrier
        sequence_barrier = std::make_unique<MockSequenceBarrier>();

        // Khởi tạo ring buffer với kích thước BUFFER_SIZE
        ring_buffer = std::make_unique<RingBuffer<TestEvent, BUFFER_SIZE> >([]() { return createTestEvent(); });

        // Khởi tạo giá trị cho ring buffer
        for (size_t i = 0; i < BUFFER_SIZE; i++) {
            TestEvent &event = ring_buffer->get(i);
            event.value = static_cast<long>(i * 10);
            event.message = "Event " + std::to_string(i);
        }

        // Khởi tạo event handler mặc định
        event_handler = [this](TestEvent &event, size_t sequence, bool end_of_batch) {
            processed_values.push_back(event.value);
            processed_messages.push_back(event.message);
            processed_sequences.push_back(sequence);
            if (end_of_batch) {
                batch_ends.push_back(sequence);
            }
        };
    }

    static constexpr size_t BUFFER_SIZE = 16;
    std::unique_ptr<MockSequenceBarrier> sequence_barrier;
    std::unique_ptr<RingBuffer<TestEvent, BUFFER_SIZE> > ring_buffer;
    std::function<void(TestEvent &, size_t, bool)> event_handler;

    // Lưu thông tin về các sự kiện đã được xử lý
    std::vector<long> processed_values;
    std::vector<std::string> processed_messages;
    std::vector<size_t> processed_sequences;
    std::vector<size_t> batch_ends;
};

// Test khởi tạo BatchEventProcessor
TEST_F(BatchEventProcessorTest, Initialization) {
    BatchEventProcessor<TestEvent, BUFFER_SIZE> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Kiểm tra giá trị ban đầu của sequence (theo Util::calculate_initial_value_sequence)
    EXPECT_EQ(processor.get_cursor().get_with_acquire(), BUFFER_SIZE);
}

// Test xử lý một batch đơn sự kiện
TEST_F(BatchEventProcessorTest, ProcessSingleBatch) {
    BatchEventProcessor<TestEvent, BUFFER_SIZE> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được gọi với sequence BUFFER_SIZE+1
    // Và trả về sequence BUFFER_SIZE+4 (cho biết có 4 sự kiện có sẵn)
    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE+1))
            .WillOnce(Return(BUFFER_SIZE + 4));

    // Mong đợi sequence barrier sẽ được gọi với sequence BUFFER_SIZE+5
    // Nhưng sẽ throw AlertException để dừng vòng lặp
    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 5))
            .WillOnce(Throw(AlertException()));

    // Mong đợi sequence barrier sẽ được clear alert ban đầu
    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Chạy processor trong thread riêng
    std::thread processor_thread([&processor]() {
        processor.run();
    });

    // Đợi để đảm bảo thread đã chạy
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor_thread.join();

    // Kiểm tra kết quả
    ASSERT_EQ(processed_values.size(), 4);
    EXPECT_EQ(processed_values[0], 10);
    EXPECT_EQ(processed_values[1], 20);
    EXPECT_EQ(processed_values[2], 30);
    EXPECT_EQ(processed_values[3], 40);

    ASSERT_EQ(processed_messages.size(), 4);
    EXPECT_EQ(processed_messages[0], "Event 1");
    EXPECT_EQ(processed_messages[1], "Event 2");
    EXPECT_EQ(processed_messages[2], "Event 3");
    EXPECT_EQ(processed_messages[3], "Event 4");

    ASSERT_EQ(processed_sequences.size(), 4);
    EXPECT_EQ(processed_sequences[0], BUFFER_SIZE + 1);
    EXPECT_EQ(processed_sequences[1], BUFFER_SIZE + 2);
    EXPECT_EQ(processed_sequences[2], BUFFER_SIZE + 3);
    EXPECT_EQ(processed_sequences[3], BUFFER_SIZE + 4);

    // Chỉ sequence cuối cùng được đánh dấu là end_of_batch
    ASSERT_EQ(batch_ends.size(), 1);
    EXPECT_EQ(batch_ends[0], BUFFER_SIZE + 4);

    // Kiểm tra cursor đã được cập nhật
    EXPECT_EQ(processor.get_cursor().get_with_acquire(), BUFFER_SIZE + 4);
}

// Test xử lý nhiều batch sự kiện
TEST_F(BatchEventProcessorTest, ProcessMultipleBatches) {
    BatchEventProcessor<TestEvent, BUFFER_SIZE> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được gọi 3 lần với các sequence khác nhau
    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 1))
            .WillOnce(Return(BUFFER_SIZE + 3)); // Batch 1: sequences BUFFER_SIZE+1 đến BUFFER_SIZE+3

    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 4))
            .WillOnce(Return(BUFFER_SIZE + 6)); // Batch 2: sequences BUFFER_SIZE+4 đến BUFFER_SIZE+6

    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 7))
            .WillOnce(Throw(AlertException())); // Dừng vòng lặp

    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Chạy processor trong thread riêng
    std::thread processor_thread([&processor]() {
        processor.run();
    });

    // Đợi để đảm bảo thread đã chạy
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor_thread.join();

    // Kiểm tra kết quả
    ASSERT_EQ(processed_values.size(), 6);
    EXPECT_EQ(processed_values[0], 10);
    EXPECT_EQ(processed_values[1], 20);
    EXPECT_EQ(processed_values[2], 30);
    EXPECT_EQ(processed_values[3], 40);
    EXPECT_EQ(processed_values[4], 50);
    EXPECT_EQ(processed_values[5], 60);

    ASSERT_EQ(processed_messages.size(), 6);
    for (size_t i = 0; i < 6; i++) {
        EXPECT_EQ(processed_messages[i], "Event " + std::to_string(i + 1));
    }

    // Kiểm tra batch ends
    ASSERT_EQ(batch_ends.size(), 2);
    EXPECT_EQ(batch_ends[0], BUFFER_SIZE + 3);
    EXPECT_EQ(batch_ends[1], BUFFER_SIZE + 6);

    // Kiểm tra cursor đã được cập nhật
    EXPECT_EQ(processor.get_cursor().get_with_acquire(), BUFFER_SIZE + 6);
}

// Test halt function
TEST_F(BatchEventProcessorTest, HaltProcessor) {
    BatchEventProcessor<TestEvent, BUFFER_SIZE> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được alert
    EXPECT_CALL(*sequence_barrier, alert())
            .Times(1);

    // Gọi halt
    processor.halt();
}

// Test xử lý khi không có sự kiện nào
TEST_F(BatchEventProcessorTest, ProcessNoEvents) {
    BatchEventProcessor<TestEvent, BUFFER_SIZE> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier trả về một sequence nhỏ hơn sequence yêu cầu
    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 1))
            .Times(2)
            .WillOnce(Return(BUFFER_SIZE))
            .WillOnce(Throw(AlertException()));

    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Chạy processor trong thread riêng
    std::thread processor_thread([&processor]() {
        processor.run();
    });

    // Đợi để đảm bảo thread đã chạy
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    processor_thread.join();

    // Kiểm tra không có sự kiện nào được xử lý
    EXPECT_EQ(processed_values.size(), 0);

    // Cursor không được cập nhật (vẫn giữ giá trị ban đầu)
    EXPECT_EQ(processor.get_cursor().get_with_acquire(), BUFFER_SIZE);
}

// Test với một event handler khác
TEST_F(BatchEventProcessorTest, CustomEventHandler) {
    std::atomic<long> sum{0};
    std::atomic<int> event_count{0};
    std::atomic<int> batch_count{0};

    auto custom_handler = [&](TestEvent &event, size_t sequence, bool end_of_batch) {
        sum += event.value;
        event_count++;
        if (end_of_batch) {
            batch_count++;
        }
    };

    BatchEventProcessor<TestEvent, BUFFER_SIZE> processor(*sequence_barrier, custom_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được gọi
    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 1))
            .WillOnce(Return(BUFFER_SIZE + 5)); // Sequences BUFFER_SIZE+1 đến BUFFER_SIZE+5

    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 6))
            .WillOnce(Throw(AlertException())); // Dừng vòng lặp

    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Chạy processor trong thread riêng
    std::thread processor_thread([&processor]() {
        processor.run();
    });

    // Đợi để đảm bảo thread đã chạy
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor_thread.join();

    // Kiểm tra kết quả
    EXPECT_EQ(event_count, 5); // 5 sự kiện được xử lý
    EXPECT_EQ(batch_count, 1); // 1 batch

    // Tính tổng giá trị từ (BUFFER_SIZE+1)*10 đến (BUFFER_SIZE+5)*10
    long expected_sum = 0;
    for (size_t i = 1; i <= 5; i++) {
        expected_sum += i * 10;
    }
    EXPECT_EQ(sum, expected_sum);

    // Kiểm tra cursor đã được cập nhật
    EXPECT_EQ(processor.get_cursor().get_with_acquire(), BUFFER_SIZE + 5);
}

// Test xử lý khi event handler ném ngoại lệ
TEST_F(BatchEventProcessorTest, ExceptionInEventHandler) {
    bool exception_thrown = false;

    auto exception_handler = [&](TestEvent &event, size_t sequence, bool end_of_batch) {
        if (sequence == BUFFER_SIZE + 3) {
            exception_thrown = true;
            throw std::runtime_error("Test exception in event handler");
        }
        processed_values.push_back(event.value);
        processed_sequences.push_back(sequence);
    };

    BatchEventProcessor<TestEvent, BUFFER_SIZE> processor(*sequence_barrier, exception_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được gọi
    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 1))
            .WillOnce(Return(BUFFER_SIZE + 4)); // Sequences BUFFER_SIZE+1 đến BUFFER_SIZE+4

    // Không cần mong đợi thêm lệnh gọi vì processor sẽ dừng sau khi bắt ngoại lệ

    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Chạy processor
    processor.run();

    // Kiểm tra kết quả
    EXPECT_TRUE(exception_thrown);
    ASSERT_EQ(processed_values.size(), 2); // Chỉ 2 sự kiện đầu tiên được xử lý
    EXPECT_EQ(processed_values[0], 10);
    EXPECT_EQ(processed_values[1], 20);

    // Kiểm tra cursor (chỉ được cập nhật đến điểm cuối của batch trước)
    EXPECT_EQ(processor.get_cursor().get_with_acquire(), BUFFER_SIZE); // Không có batch nào hoàn thành
}

// Test với batch size lớn hơn ring buffer size
TEST_F(BatchEventProcessorTest, BatchSizeLargerThanRingBuffer) {
    BatchEventProcessor<TestEvent, BUFFER_SIZE> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier trả về một giá trị lớn hơn ring buffer size
    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 1))
            .WillOnce(Return(BUFFER_SIZE + 20)); // Lớn hơn ring buffer size (BUFFER_SIZE)

    // Mong đợi sequence barrier sẽ được gọi lại
    EXPECT_CALL(*sequence_barrier, wait_for(BUFFER_SIZE + 21))
            .WillOnce(Throw(AlertException())); // Dừng vòng lặp

    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Chạy processor trong thread riêng
    std::thread processor_thread([&processor]() {
        processor.run();
    });

    // Đợi để đảm bảo thread đã chạy
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor_thread.join();

    // Kiểm tra kết quả (ring buffer size KHÔNG giới hạn số lượng sự kiện thực tế được xử lý)
    ASSERT_EQ(processed_values.size(), 20);

    // Kiểm tra cursor đã được cập nhật
    EXPECT_EQ(processor.get_cursor().get_with_acquire(), BUFFER_SIZE + 20);
}
