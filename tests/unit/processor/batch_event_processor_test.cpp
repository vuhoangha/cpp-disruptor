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
class MockSequenceBarrier : public SequenceBarrier {
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

        // Khởi tạo ring buffer với kích thước 16
        ring_buffer = std::make_unique<RingBuffer<TestEvent, BUFFER_SIZE_T> >([]() { return createTestEvent(); });

        // Khởi tạo giá trị cho ring buffer
        for (size_t i = 0; i < BUFFER_SIZE_T; i++) {
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

    static constexpr size_t BUFFER_SIZE_T = 16;
    std::unique_ptr<MockSequenceBarrier> sequence_barrier;
    std::unique_ptr<RingBuffer<TestEvent, BUFFER_SIZE_T> > ring_buffer;
    std::function<void(TestEvent &, size_t, bool)> event_handler;

    // Lưu thông tin về các sự kiện đã được xử lý
    std::vector<long> processed_values;
    std::vector<std::string> processed_messages;
    std::vector<size_t> processed_sequences;
    std::vector<size_t> batch_ends;
};

// Test khởi tạo BatchEventProcessor
TEST_F(BatchEventProcessorTest, Initialization) {
    BatchEventProcessor<TestEvent, BUFFER_SIZE_T> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Kiểm tra giá trị ban đầu của sequence (theo mặc định là -1 với ring buffer size 16)
    EXPECT_EQ(processor.get_cursor().get(), BUFFER_SIZE_T);
}

// Test xử lý một batch đơn sự kiện
TEST_F(BatchEventProcessorTest, ProcessSingleBatch) {
    BatchEventProcessor<TestEvent, 16> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được gọi với sequence 0
    // Và trả về sequence 3 (cho biết có 4 sự kiện có sẵn từ 0-3)
    EXPECT_CALL(*sequence_barrier, wait_for(0))
            .WillOnce(Return(3));

    // Mong đợi sequence barrier sẽ được gọi với sequence 4
    // Nhưng sẽ throw AlertException để dừng vòng lặp
    EXPECT_CALL(*sequence_barrier, wait_for(4))
            .WillOnce(Throw(AlertException()));

    // Mong đợi sequence barrier sẽ bị kiểm tra alert
    EXPECT_CALL(*sequence_barrier, check_alert())
            .Times(AtLeast(1));

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
    EXPECT_EQ(processed_values[0], 0);
    EXPECT_EQ(processed_values[1], 10);
    EXPECT_EQ(processed_values[2], 20);
    EXPECT_EQ(processed_values[3], 30);

    ASSERT_EQ(processed_messages.size(), 4);
    EXPECT_EQ(processed_messages[0], "Event 0");
    EXPECT_EQ(processed_messages[1], "Event 1");
    EXPECT_EQ(processed_messages[2], "Event 2");
    EXPECT_EQ(processed_messages[3], "Event 3");

    ASSERT_EQ(processed_sequences.size(), 4);
    EXPECT_EQ(processed_sequences[0], 0);
    EXPECT_EQ(processed_sequences[1], 1);
    EXPECT_EQ(processed_sequences[2], 2);
    EXPECT_EQ(processed_sequences[3], 3);

    // Chỉ sequence cuối cùng (3) được đánh dấu là end_of_batch
    ASSERT_EQ(batch_ends.size(), 1);
    EXPECT_EQ(batch_ends[0], 3);

    // Kiểm tra cursor đã được cập nhật
    EXPECT_EQ(processor.get_cursor().get(), 3);
}

// Test xử lý nhiều batch sự kiện
TEST_F(BatchEventProcessorTest, ProcessMultipleBatches) {
    BatchEventProcessor<TestEvent, 16> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được gọi 3 lần với các sequence khác nhau
    EXPECT_CALL(*sequence_barrier, wait_for(0))
            .WillOnce(Return(2)); // Batch 1: sequences 0-2

    EXPECT_CALL(*sequence_barrier, wait_for(3))
            .WillOnce(Return(5)); // Batch 2: sequences 3-5

    EXPECT_CALL(*sequence_barrier, wait_for(6))
            .WillOnce(Throw(AlertException())); // Dừng vòng lặp

    // Mong đợi các lệnh gọi khác
    EXPECT_CALL(*sequence_barrier, check_alert())
            .Times(AtLeast(1));

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
    EXPECT_EQ(processed_values[0], 0);
    EXPECT_EQ(processed_values[1], 10);
    EXPECT_EQ(processed_values[2], 20);
    EXPECT_EQ(processed_values[3], 30);
    EXPECT_EQ(processed_values[4], 40);
    EXPECT_EQ(processed_values[5], 50);

    ASSERT_EQ(processed_messages.size(), 6);
    for (size_t i = 0; i < 6; i++) {
        EXPECT_EQ(processed_messages[i], "Event " + std::to_string(i));
    }

    // Kiểm tra batch ends
    ASSERT_EQ(batch_ends.size(), 2);
    EXPECT_EQ(batch_ends[0], 2);
    EXPECT_EQ(batch_ends[1], 5);

    // Kiểm tra cursor đã được cập nhật
    EXPECT_EQ(processor.get_cursor().get(), 5);
}

// Test halt function
TEST_F(BatchEventProcessorTest, HaltProcessor) {
    BatchEventProcessor<TestEvent, 16> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được alert
    EXPECT_CALL(*sequence_barrier, alert())
            .Times(1);

    // Gọi halt
    processor.halt();
}

// Test xử lý khi không có sự kiện nào
TEST_F(BatchEventProcessorTest, ProcessNoEvents) {
    BatchEventProcessor<TestEvent, 16> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier trả về một sequence nhỏ hơn sequence yêu cầu
    EXPECT_CALL(*sequence_barrier, wait_for(0))
            .WillOnce(Return(-1)); // Không có sự kiện nào

    // Mong đợi sequence barrier sẽ được gọi lại với sequence 0
    EXPECT_CALL(*sequence_barrier, wait_for(0))
            .WillOnce(Throw(AlertException())); // Dừng vòng lặp

    // Mong đợi các lệnh gọi khác
    EXPECT_CALL(*sequence_barrier, check_alert())
            .Times(AtLeast(1));

    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Chạy processor trong thread riêng
    std::thread processor_thread([&processor]() {
        processor.run();
    });

    // Đợi để đảm bảo thread đã chạy
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor_thread.join();

    // Kiểm tra không có sự kiện nào được xử lý
    EXPECT_EQ(processed_values.size(), 0);

    // Cursor không được cập nhật (vẫn giữ giá trị ban đầu)
    EXPECT_EQ(processor.get_cursor().get(), -1);
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

    BatchEventProcessor<TestEvent, 16> processor(*sequence_barrier, custom_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được gọi
    EXPECT_CALL(*sequence_barrier, wait_for(0))
            .WillOnce(Return(4)); // Sequences 0-4

    EXPECT_CALL(*sequence_barrier, wait_for(5))
            .WillOnce(Throw(AlertException())); // Dừng vòng lặp

    // Mong đợi các lệnh gọi khác
    EXPECT_CALL(*sequence_barrier, check_alert())
            .Times(AtLeast(1));

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
    EXPECT_EQ(sum, 0 + 10 + 20 + 30 + 40); // Tổng giá trị

    // Kiểm tra cursor đã được cập nhật
    EXPECT_EQ(processor.get_cursor().get(), 4);
}

// Test xử lý khi event handler ném ngoại lệ
TEST_F(BatchEventProcessorTest, ExceptionInEventHandler) {
    bool exception_thrown = false;

    auto exception_handler = [&](TestEvent &event, size_t sequence, bool end_of_batch) {
        if (sequence == 2) {
            exception_thrown = true;
            throw std::runtime_error("Test exception in event handler");
        }
        processed_values.push_back(event.value);
        processed_sequences.push_back(sequence);
    };

    BatchEventProcessor<TestEvent, 16> processor(*sequence_barrier, exception_handler, *ring_buffer);

    // Mong đợi sequence barrier sẽ được gọi
    EXPECT_CALL(*sequence_barrier, wait_for(0))
            .WillOnce(Return(3)); // Sequences 0-3

    // Không cần mong đợi thêm lệnh gọi vì processor sẽ dừng sau khi bắt ngoại lệ

    // Mong đợi các lệnh gọi khác
    EXPECT_CALL(*sequence_barrier, check_alert())
            .Times(AtLeast(1));

    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Chạy processor
    processor.run();

    // Kiểm tra kết quả
    EXPECT_TRUE(exception_thrown);
    ASSERT_EQ(processed_values.size(), 2); // Chỉ 2 sự kiện đầu tiên được xử lý
    EXPECT_EQ(processed_values[0], 0);
    EXPECT_EQ(processed_values[1], 10);

    // Kiểm tra cursor (chỉ được cập nhật đến điểm cuối của batch trước)
    EXPECT_EQ(processor.get_cursor().get(), -1); // Không có batch nào hoàn thành
}

// Test với batch size lớn hơn ring buffer size
TEST_F(BatchEventProcessorTest, BatchSizeLargerThanRingBuffer) {
    BatchEventProcessor<TestEvent, 16> processor(*sequence_barrier, event_handler, *ring_buffer);

    // Mong đợi sequence barrier trả về một giá trị lớn hơn ring buffer size
    EXPECT_CALL(*sequence_barrier, wait_for(0))
            .WillOnce(Return(20)); // Lớn hơn ring buffer size (16)

    // Mong đợi sequence barrier sẽ được gọi lại
    EXPECT_CALL(*sequence_barrier, wait_for(21))
            .WillOnce(Throw(AlertException())); // Dừng vòng lặp

    // Mong đợi các lệnh gọi khác
    EXPECT_CALL(*sequence_barrier, check_alert())
            .Times(AtLeast(1));

    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Chạy processor trong thread riêng
    std::thread processor_thread([&processor]() {
        processor.run();
    });

    // Đợi để đảm bảo thread đã chạy
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    processor_thread.join();

    // Kiểm tra kết quả (ring buffer size giới hạn số lượng sự kiện thực tế được xử lý)
    ASSERT_EQ(processed_values.size(), 16);

    // Kiểm tra cursor đã được cập nhật
    EXPECT_EQ(processor.get_cursor().get(), 20);
}

// Test tích hợp với thread thực
TEST_F(BatchEventProcessorTest, IntegrationWithRealThread) {
    // Tạo một real sequence barrier thay vì mock
    class SimpleSequenceBarrier : public SequenceBarrier {
    private:
        std::atomic<bool> alerted{false};
        std::atomic<size_t> sequence{0};

    public:
        size_t wait_for(size_t seq) override {
            check_alert();
            // Đơn giản trả về sequence hiện tại
            return sequence.load();
        }

        bool is_alerted() const override {
            return alerted.load();
        }

        void alert() override {
            alerted.store(true);
        }

        void clear_alert() override {
            alerted.store(false);
        }

        void check_alert() const override {
            if (alerted.load()) {
                throw AlertException();
            }
        }

        void update_sequence(size_t seq) {
            sequence.store(seq);
        }
    };

    // Tạo một SimpleSequenceBarrier
    auto real_barrier = std::make_shared<SimpleSequenceBarrier>();

    // Biến để theo dõi sự kiện đã xử lý
    std::mutex event_mutex;
    std::vector<long> real_processed_values;
    std::vector<std::string> real_processed_messages;
    std::atomic<bool> processing_complete{false};

    // Event handler
    auto real_handler = [&](TestEvent &event, size_t sequence, bool end_of_batch) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Mô phỏng xử lý
        std::lock_guard<std::mutex> lock(event_mutex);
        real_processed_values.push_back(event.value);
        real_processed_messages.push_back(event.message);
    };

    // Tạo processor
    BatchEventProcessor<TestEvent, 16> processor(*real_barrier, real_handler, *ring_buffer);

    // Chạy processor trong thread riêng
    std::thread processor_thread([&processor, &processing_complete]() {
        processor.run();
        processing_complete.store(true);
    });

    // Cập nhật sequence trong một thread riêng để mô phỏng các sự kiện mới
    std::thread update_thread([&real_barrier]() {
        for (size_t i = 0; i < 5; i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            real_barrier->update_sequence(i);
        }

        // Đợi để đảm bảo tất cả sự kiện được xử lý
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Dừng processor
        real_barrier->alert();
    });

    // Đợi cho đến khi xử lý hoàn tất
    update_thread.join();
    processor_thread.join();

    // Kiểm tra kết quả
    std::lock_guard<std::mutex> lock(event_mutex);
    ASSERT_EQ(real_processed_values.size(), 5);
    EXPECT_TRUE(processing_complete);

    // Kiểm tra giá trị
    for (size_t i = 0; i < real_processed_values.size(); i++) {
        EXPECT_EQ(real_processed_values[i], i * 10);
        EXPECT_EQ(real_processed_messages[i], "Event " + std::to_string(i));
    }

    // Kiểm tra cursor
    EXPECT_EQ(processor.get_cursor().get(), 4);
}

// Test hiệu năng của batch processing
TEST_F(BatchEventProcessorTest, BatchProcessingPerformance) {
    // Đặt giá trị các sự kiện để test hiệu năng
    for (size_t i = 0; i < 16; i++) {
        TestEvent &event = ring_buffer->get(i);
        event.value = static_cast<long>(i);
        event.message = "Performance test event " + std::to_string(i);
    }

    // Đếm số lần event handler được gọi
    std::atomic<int> handler_call_count{0};

    // Event handler đơn giản
    auto perf_handler = [&](TestEvent &event, size_t sequence, bool end_of_batch) {
        handler_call_count++;
    };

    BatchEventProcessor<TestEvent, 16> processor(*sequence_barrier, perf_handler, *ring_buffer);

    // Thiết lập kỳ vọng - một batch lớn với 15 sự kiện
    EXPECT_CALL(*sequence_barrier, wait_for(0))
            .WillOnce(Return(14)); // Sequences 0-14

    EXPECT_CALL(*sequence_barrier, wait_for(15))
            .WillOnce(Throw(AlertException())); // Dừng vòng lặp

    EXPECT_CALL(*sequence_barrier, check_alert())
            .Times(AtLeast(1));

    EXPECT_CALL(*sequence_barrier, clear_alert())
            .Times(1);

    // Đo thời gian xử lý
    auto start_time = std::chrono::high_resolution_clock::now();

    // Chạy processor
    processor.run();

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);

    // Kiểm tra kết quả
    EXPECT_EQ(handler_call_count, 15); // 15 sự kiện được xử lý
    EXPECT_EQ(processor.get_cursor().get(), 14);

    // Ghi chú về thời gian xử lý (không phải là test thực sự)
    std::cout << "Batch processing time for 15 events: " << duration.count() << " microseconds" << std::endl;
}
