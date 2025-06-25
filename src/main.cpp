#include <iomanip>
#include <iostream>

#include "../include/processor/BatchEventProcessor.hpp"
#include "event.hpp"
#include "../include/sequencer/MultiProducerSequencer.hpp"
#include "../include/barriers/ProcessingSequenceBarrier.hpp"
#include "../include/ring_buffer/RingBuffer.hpp"
#include "../include/sequencer/SingleProducerSequencer.hpp"
#include "../include/sequence/Sequence.hpp"
#include "../include/common/Util.hpp"
#include "../include/wait_strategy/WaitStrategyType.hpp"


void run_single_sequencer() {
    constexpr size_t ring_buffer_size = 4;
    disruptor::RingBuffer<disruptor::Event, ring_buffer_size> ring_buffer([]() { return disruptor::Event(); });

    disruptor::SingleProducerSequencer<disruptor::Event, ring_buffer_size, 1> sequencer(ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_sequencer = sequencer.get_cursor();


    // Tạo đối tượng BatchEventProcessor
    auto eventHandler_1 = [](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        std::cout << std::format("{:%F %T}", std::chrono::system_clock::now()) << "Process A - event on sequence: " <<
                sequence << " - value: " << event.get_value();
        if (endOfBatch) {
            std::cout << " (end of batch)";
        }
        std::cout << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    };
    constexpr size_t NUMBER_DEPENDENT_SEQUENCES = 1;
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_1(
        true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, ring_buffer_size> processor_1(
        sequence_barrier_1, eventHandler_1, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_1 = processor_1.get_cursor();
    sequencer.add_gating_sequences({cursor_batch_event_processor_1});
    std::thread processorThread_1([&processor_1]() { processor_1.run(); });


    // Tạo đối tượng BatchEventProcessor
    auto eventHandler_2 = [](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        std::cout << std::format("{:%F %T}", std::chrono::system_clock::now()) << "Process B - event on sequence: " <<
                sequence << " - value: " << event.get_value();
        if (endOfBatch) {
            std::cout << " (end of batch)";
        }
        std::cout << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(2678));
    };
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_2(
        false, {cursor_batch_event_processor_1}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, ring_buffer_size> processor_2(
        sequence_barrier_2, eventHandler_2, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_2 = processor_2.get_cursor();
    sequencer.add_gating_sequences({cursor_batch_event_processor_2});
    std::thread processorThread_2([&processor_2]() { processor_2.run(); });


    std::this_thread::sleep_for(std::chrono::seconds(1));


    while (true) {
        const size_t claimed_sequence = sequencer.next(1);
        disruptor::Event &event = ring_buffer.get(claimed_sequence);
        event.set_value(claimed_sequence);
        sequencer.publish(claimed_sequence);
    }

    std::this_thread::sleep_for(std::chrono::seconds(3600));
}


void run_multiple_sequencer() {
    constexpr size_t ring_buffer_size = 4;
    disruptor::RingBuffer<disruptor::Event, ring_buffer_size> ring_buffer([]() { return disruptor::Event(); });

    disruptor::MultiProducerSequencer<disruptor::Event, ring_buffer_size, 1> sequencer(ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_sequencer = sequencer.get_cursor();

    constexpr size_t NUMBER_DEPENDENT_SEQUENCES = 1;
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier(
        true, {cursor_sequencer}, sequencer);

    // Tạo một hàm xử lý sự kiện
    auto eventHandler = [](disruptor::Event &event, const size_t sequence, const bool endOfBatch) {
        std::cout << "Process event on sequence: " << sequence << " - value: " << event.get_value();
        if (endOfBatch) {
            std::cout << " (end of batch)";
        }
        std::cout << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };

    // Tạo đối tượng BatchEventProcessor
    disruptor::BatchEventProcessor<disruptor::Event, ring_buffer_size> processor(
        sequence_barrier, eventHandler, ring_buffer);

    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor = processor.get_cursor();
    sequencer.add_gating_sequences({cursor_batch_event_processor});

    // Có thể chạy processor trong một thread riêng
    std::thread consumerThread([&processor]() { processor.run(); });

    std::this_thread::sleep_for(std::chrono::seconds(1));


    std::thread producer_1_thread([&sequencer, &ring_buffer]() {
        while (true) {
            const size_t claimed_sequence = sequencer.next(1);
            disruptor::Event &event = ring_buffer.get(claimed_sequence);
            event.set_value(claimed_sequence);
            sequencer.publish(claimed_sequence);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });

    std::thread producer_2_thread([&sequencer, &ring_buffer]() {
        while (true) {
            const size_t claimed_sequence = sequencer.next(1);
            disruptor::Event &event = ring_buffer.get(claimed_sequence);
            event.set_value(claimed_sequence);
            sequencer.publish(claimed_sequence);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });


    std::this_thread::sleep_for(std::chrono::seconds(3600));
}


void test_1_producer_1_consumer() {
    constexpr size_t ring_buffer_size = 1024;
    disruptor::RingBuffer<disruptor::Event, ring_buffer_size> ring_buffer([]() { return disruptor::Event(); });

    disruptor::SingleProducerSequencer<disruptor::Event, ring_buffer_size, 1> sequencer(ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_sequencer = sequencer.get_cursor();


    // Tạo đối tượng BatchEventProcessor
    size_t counter = 0;
    auto eventHandler_1 = [&counter](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        counter++;
    };
    constexpr size_t NUMBER_DEPENDENT_SEQUENCES = 1;
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_1(
        true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, ring_buffer_size> processor_1(
        sequence_barrier_1, eventHandler_1, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_1 = processor_1.get_cursor();
    sequencer.add_gating_sequences({cursor_batch_event_processor_1});
    std::thread processorThread_1([&processor_1]() { processor_1.run(); });

    // Số lượng sự kiện sẽ được gửi
    constexpr size_t NUM_EVENTS = 10'000'000'004;

    // Đợi consumer khởi động
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // Đo thời gian và hiệu suất của producer
    std::cout << "Start send " << NUM_EVENTS << " event..." << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();


    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        const size_t claimed_sequence = sequencer.next(1);
        disruptor::Event &event = ring_buffer.get(claimed_sequence);
        event.set_value(claimed_sequence);
        sequencer.publish(claimed_sequence);
    }


    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);


    // Đợi consumer xử lý hết
    while (counter < NUM_EVENTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    processor_1.halt();
    processorThread_1.join();


    double seconds = duration.count() / 1000.0;
    double events_per_second = NUM_EVENTS / seconds;


    // std::cout << "Hoàn thành!" << std::endl;
    // std::cout << "Số lượng sự kiện: " << NUM_EVENTS << std::endl;
    std::cout << "Time: " << std::fixed << std::setprecision(3) << seconds << " s" << std::endl;
    std::cout << "Rate: " << std::fixed << std::setprecision(0) << events_per_second << " event/s" << std::endl;
}


void test_1_producer_6_consumer() {
    constexpr size_t BUFFER_SIZE = 1024;
    constexpr size_t NUMBER_GATING_SEQUENCES = 6;
    constexpr size_t NUMBER_DEPENDENT_SEQUENCES = 1;
    constexpr size_t NUM_EVENTS = 300'000'005;

    std::cout << "BUFFER_SIZE: " << BUFFER_SIZE << std::endl;
    std::cout << "NUMBER_GATING_SEQUENCES: " << NUMBER_GATING_SEQUENCES << std::endl;
    std::cout << "NUMBER_DEPENDENT_SEQUENCES: " << NUMBER_DEPENDENT_SEQUENCES << std::endl;
    std::cout << "NUM_EVENTS: " << NUM_EVENTS << std::endl;

    disruptor::RingBuffer<disruptor::Event, BUFFER_SIZE> ring_buffer([]() { return disruptor::Event(); });
    disruptor::SingleProducerSequencer<disruptor::Event, BUFFER_SIZE, NUMBER_GATING_SEQUENCES> sequencer(ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_sequencer = sequencer.get_cursor();


    // Tạo đối tượng BatchEventProcessor 1
    size_t counter_1 = 0;
    auto eventHandler_1 = [&counter_1](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        counter_1++;
    };
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_1(
        true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, BUFFER_SIZE> processor_1(
        sequence_barrier_1, eventHandler_1, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_1 = processor_1.get_cursor();
    std::thread processorThread_1([&processor_1]() { processor_1.run(); });


    // Tạo đối tượng BatchEventProcessor 2
    size_t counter_2 = 0;
    auto eventHandler_2 = [&counter_2](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        counter_2++;
    };
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_2(
        true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, BUFFER_SIZE> processor_2(
        sequence_barrier_2, eventHandler_2, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_2 = processor_2.get_cursor();
    std::thread processorThread_2([&processor_2]() { processor_2.run(); });


    // Tạo đối tượng BatchEventProcessor 3
    size_t counter_3 = 0;
    auto eventHandler_3 = [&counter_3](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        counter_3++;
    };
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_3(
        true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, BUFFER_SIZE> processor_3(
        sequence_barrier_3, eventHandler_3, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_3 = processor_3.get_cursor();
    std::thread processorThread_3([&processor_3]() { processor_3.run(); });


    // Tạo đối tượng BatchEventProcessor 4
    size_t counter_4 = 0;
    auto eventHandler_4 = [&counter_4](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        counter_4++;
    };
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_4(
        true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, BUFFER_SIZE> processor_4(
        sequence_barrier_4, eventHandler_4, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_4 = processor_4.get_cursor();
    std::thread processorThread_4([&processor_4]() { processor_4.run(); });


    // Tạo đối tượng BatchEventProcessor 5
    size_t counter_5 = 0;
    auto eventHandler_5 = [&counter_5](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        counter_5++;
    };
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_5(
        true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, BUFFER_SIZE> processor_5(
        sequence_barrier_5, eventHandler_5, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_5 = processor_5.get_cursor();
    std::thread processorThread_5([&processor_5]() { processor_5.run(); });


    // Tạo đối tượng BatchEventProcessor 6
    size_t counter_6 = 0;
    auto eventHandler_6 = [&counter_6](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        counter_6++;
    };
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_6(
        true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, BUFFER_SIZE> processor_6(
        sequence_barrier_6, eventHandler_6, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_6 = processor_6.get_cursor();
    std::thread processorThread_6([&processor_6]() { processor_6.run(); });


    sequencer.add_gating_sequences({
        cursor_batch_event_processor_1, cursor_batch_event_processor_2, cursor_batch_event_processor_3,
        cursor_batch_event_processor_4,
        cursor_batch_event_processor_5, cursor_batch_event_processor_6
    });

    // Đợi consumer khởi động
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto start_time = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        const size_t claimed_sequence = sequencer.next(1);
        disruptor::Event &event = ring_buffer.get(claimed_sequence);
        event.set_value(claimed_sequence);
        sequencer.publish(claimed_sequence);
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);


    // wait consumer 1
    while (counter_1 < NUM_EVENTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    processor_1.halt();
    processorThread_1.join();

    // wait consumer 2
    while (counter_2 < NUM_EVENTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    processor_2.halt();
    processorThread_2.join();

    // wait consumer 3
    while (counter_3 < NUM_EVENTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    processor_3.halt();
    processorThread_3.join();

    // wait consumer 4
    while (counter_4 < NUM_EVENTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    processor_4.halt();
    processorThread_4.join();

    // wait consumer 5
    while (counter_5 < NUM_EVENTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    processor_5.halt();
    processorThread_5.join();

    // wait consumer 6
    while (counter_6 < NUM_EVENTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    processor_6.halt();
    processorThread_6.join();


    double seconds = duration.count() / 1000.0;
    double events_per_second = NUM_EVENTS / seconds;
    std::cout << "Time: " << std::fixed << std::setprecision(3) << seconds << " s" << std::endl;
    std::cout << "Rate: " << std::fixed << std::setprecision(0) << events_per_second << " event/s" << std::endl;
}


void test_3_producer_1_consumer() {
    constexpr size_t BUFFER_SIZE = 1 << 15;
    constexpr size_t NUMBER_GATING_SEQUENCES = 1;
    constexpr size_t NUMBER_DEPENDENT_SEQUENCES = 1;
    constexpr size_t NUM_EVENTS_PER_PRODUCER = 50'000'000;
    constexpr size_t NUM_PRODUCER = 5;
    constexpr size_t NUM_EVENTS = NUM_EVENTS_PER_PRODUCER * NUM_PRODUCER;

    std::cout << "BUFFER_SIZE: " << BUFFER_SIZE << std::endl;
    std::cout << "NUMBER_GATING_SEQUENCES: " << NUMBER_GATING_SEQUENCES << std::endl;
    std::cout << "NUMBER_DEPENDENT_SEQUENCES: " << NUMBER_DEPENDENT_SEQUENCES << std::endl;
    std::cout << "NUM_EVENTS_PER_PRODUCER: " << NUM_EVENTS_PER_PRODUCER << std::endl;
    std::cout << "NUM_PRODUCER: " << NUM_PRODUCER << std::endl;
    std::cout << "NUM_EVENTS: " << NUM_EVENTS << std::endl;

    disruptor::RingBuffer<disruptor::Event, BUFFER_SIZE> ring_buffer([]() { return disruptor::Event(); });
    disruptor::MultiProducerSequencer<disruptor::Event, BUFFER_SIZE, NUMBER_GATING_SEQUENCES> sequencer(ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_sequencer = sequencer.get_cursor();


    // Tạo đối tượng BatchEventProcessor 1
    size_t counter_1 = 0;
    auto eventHandler_1 = [&counter_1](disruptor::Event &event, size_t sequence, bool endOfBatch) {
        counter_1++;
    };
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_1(
        true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, BUFFER_SIZE> processor_1(
        sequence_barrier_1, eventHandler_1, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_1 = processor_1.get_cursor();
    std::thread processorThread_1([&processor_1]() { processor_1.run(); });


    sequencer.add_gating_sequences({
        cursor_batch_event_processor_1
    });

    // Đợi consumer khởi động
    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    auto start_time = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> producer_threads;
    for (int i = 0; i < NUM_PRODUCER; ++i) {
        producer_threads.emplace_back([&sequencer, &ring_buffer, &NUM_EVENTS_PER_PRODUCER, i]() {
            std::cout << "Producer " << i << " started" << std::endl;
            for (size_t j = 0; j < NUM_EVENTS_PER_PRODUCER; ++j) {
                const size_t claimed_sequence = sequencer.next(1);
                disruptor::Event &event = ring_buffer.get(claimed_sequence);
                event.set_value(claimed_sequence);
                sequencer.publish(claimed_sequence);
            }
            std::cout << "Producer " << i << " finished" << std::endl;
        });
    }
    for (auto &thread: producer_threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);


    // wait consumer 1
    while (counter_1 < NUM_EVENTS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    processor_1.halt();
    processorThread_1.join();


    double seconds = duration.count() / 1000.0;
    double events_per_second = NUM_EVENTS / seconds;
    std::cout.imbue(std::locale("en_US.UTF-8"));
    std::cout << "Time: " << std::fixed << std::setprecision(3) << seconds << " s" << std::endl;
    std::cout << "Rate: " << std::fixed << std::setprecision(0) << events_per_second << " event/s" << std::endl;
}


void test_atomic() {
    constexpr uint64_t NUM_ITERATIONS = 500'000'000; // 1 tỷ
    std::atomic<uint64_t> counter{0};

    auto start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < NUM_ITERATIONS; ++i) {
        // counter.fetch_add(1, std::memory_order_relaxed);
        // counter.fetch_add(1, std::memory_order_release);
        // counter.store(i, std::memory_order_release);
        // counter.load(std::memory_order_relaxed);
        // uint64_t aa = counter.load(std::memory_order_acquire);

        // std::atomic_thread_fence(std::memory_order_release);
        // size_t result;
        // __asm__ __volatile__ (
        //     "lock xaddq %0, %1"
        //     : "=r" (result), "+m" (value)
        //     : "0" (1)
        //     : "memory"
        // );
        // int a = 1;

        // size_t value1 = value;
        // std::atomic_thread_fence(std::memory_order_acquire);

        // std::atomic_thread_fence(std::memory_order_release);
        // value = i;
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double seconds = elapsed.count();

    double rate = NUM_ITERATIONS / seconds;

    // Thiết lập locale để in số có dấu phẩy phân tách
    std::cout.imbue(std::locale("en_US.UTF-8"));
    std::cout << "Processed: " << NUM_ITERATIONS << " fetch_add calls in "
            << seconds << " seconds\n";
    std::cout << "Rate: " << std::fixed << std::setprecision(0)
            << rate << " ops/sec" << std::endl;
}


void test_custom_atomic() {
    constexpr uint64_t NUM_ITERATIONS = 500'000'000; // 1 tỷ
    volatile int counter = 0;

    auto start = std::chrono::high_resolution_clock::now();

    for (uint64_t i = 0; i < NUM_ITERATIONS; ++i) {
        // counter.fetch_add(1, std::memory_order_relaxed);
        // counter.fetch_add(1, std::memory_order_release);
        // counter.store(i, std::memory_order_release);
        // counter.load(std::memory_order_relaxed);
        // __atomic_fetch_add(&counter, 1, __ATOMIC_RELAXED);
        // __sync_fetch_and_add(&counter, 1);

        int result;
        asm volatile (
            "movl $1, %0\n\t"
            "lock xaddl %0, %1"
            : "=&r" (result), "+m" (counter)
            :
            : "memory"
        );
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    double seconds = elapsed.count();

    double rate = NUM_ITERATIONS / seconds;

    // Thiết lập locale để in số có dấu phẩy phân tách
    std::cout.imbue(std::locale("en_US.UTF-8"));
    std::cout << "Processed: " << NUM_ITERATIONS << " fetch_add calls in "
            << seconds << " seconds\n";
    std::cout << "Rate: " << std::fixed << std::setprecision(0)
            << rate << " ops/sec" << std::endl;
}


int main() {
    // kiểm tra hệ thống có đủ điều kiện ko
    disruptor::Util::require_for_system_run_stable();


    // run_single_sequencer();
    test_3_producer_1_consumer();
    // test_1_producer_6_consumer();
    // test_atomic();
    // test_custom_atomic();

    return 0;
}
