#include <iostream>

#include "BatchEventProcessor.hpp"
#include "event.hpp"
#include "MultiProducerSequencer.hpp"
#include "ProcessingSequenceBarrier.hpp"
#include "RingBuffer.hpp"
#include "SingleProducerSequencer.hpp"
#include "../include/Sequence.hpp"
#include "../include/Util.hpp"
#include "../include/WaitStrategyType.hpp"


void run_single_sequencer() {
    constexpr int64_t ring_buffer_size = 4;
    disruptor::RingBuffer<disruptor::Event, ring_buffer_size> ring_buffer([]() { return disruptor::Event(); });

    disruptor::SingleProducerSequencer<disruptor::Event, ring_buffer_size, 1> sequencer(ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_sequencer = sequencer.getCursor();


    // Tạo đối tượng BatchEventProcessor
    auto eventHandler_1 = [](disruptor::Event &event, int64_t sequence, bool endOfBatch) {
        std::cout << std::format("{:%F %T}", std::chrono::system_clock::now()) << "Process A - event on sequence: " << sequence << " - value: " << event.getValue();
        if (endOfBatch) {
            std::cout << " (end of batch)";
        }
        std::cout << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    };
    constexpr size_t NUMBER_DEPENDENT_SEQUENCES = 1;
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_1(true, {cursor_sequencer}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, ring_buffer_size> processor_1(sequence_barrier_1, eventHandler_1, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_1 = processor_1.getCursor();
    sequencer.addGatingSequences({cursor_batch_event_processor_1});
    std::thread processorThread_1([&processor_1]() { processor_1.run(); });


    // Tạo đối tượng BatchEventProcessor
    auto eventHandler_2 = [](disruptor::Event &event, int64_t sequence, bool endOfBatch) {
        std::cout << std::format("{:%F %T}", std::chrono::system_clock::now()) << "Process B - event on sequence: " << sequence << " - value: " << event.getValue();
        if (endOfBatch) {
            std::cout << " (end of batch)";
        }
        std::cout << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(2678));
    };
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier_2(false, {cursor_batch_event_processor_1}, sequencer);
    disruptor::BatchEventProcessor<disruptor::Event, ring_buffer_size> processor_2(sequence_barrier_2, eventHandler_2, ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor_2 = processor_2.getCursor();
    sequencer.addGatingSequences({cursor_batch_event_processor_2});
    std::thread processorThread_2([&processor_2]() { processor_2.run(); });


    std::this_thread::sleep_for(std::chrono::seconds(1));


    while (true) {
        const int64_t claimed_sequence = sequencer.next();
        disruptor::Event &event = ring_buffer.get(claimed_sequence);
        event.setValue(claimed_sequence);
        sequencer.publish(claimed_sequence);
    }

    std::this_thread::sleep_for(std::chrono::seconds(3600));
}


void run_multiple_sequencer() {
    constexpr int64_t ring_buffer_size = 4;
    disruptor::RingBuffer<disruptor::Event, ring_buffer_size> ring_buffer([]() { return disruptor::Event(); });

    disruptor::MultiProducerSequencer<disruptor::Event, ring_buffer_size, 1> sequencer(ring_buffer);
    std::reference_wrapper<disruptor::Sequence> cursor_sequencer = sequencer.getCursor();

    constexpr size_t NUMBER_DEPENDENT_SEQUENCES = 1;
    disruptor::ProcessingSequenceBarrier<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> sequence_barrier(true, {cursor_sequencer}, sequencer);

    // Tạo một hàm xử lý sự kiện
    auto eventHandler = [](disruptor::Event &event, const int64_t sequence, const bool endOfBatch) {
        std::cout << "Process event on sequence: " << sequence << " - value: " << event.getValue();
        if (endOfBatch) {
            std::cout << " (end of batch)";
        }
        std::cout << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    };

    // Tạo đối tượng BatchEventProcessor
    disruptor::BatchEventProcessor<disruptor::Event, ring_buffer_size> processor(sequence_barrier, eventHandler, ring_buffer);

    std::reference_wrapper<disruptor::Sequence> cursor_batch_event_processor = processor.getCursor();
    sequencer.addGatingSequences({cursor_batch_event_processor});

    // Có thể chạy processor trong một thread riêng
    std::thread consumerThread([&processor]() { processor.run(); });

    std::this_thread::sleep_for(std::chrono::seconds(1));


    std::thread producer_1_thread([&sequencer, &ring_buffer]() {
        while (true) {
            const int64_t claimed_sequence = sequencer.next();
            disruptor::Event &event = ring_buffer.get(claimed_sequence);
            event.setValue(claimed_sequence);
            sequencer.publish(claimed_sequence);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });

    std::thread producer_2_thread([&sequencer, &ring_buffer]() {
        while (true) {
            const int64_t claimed_sequence = sequencer.next();
            disruptor::Event &event = ring_buffer.get(claimed_sequence);
            event.setValue(claimed_sequence);
            sequencer.publish(claimed_sequence);

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    });


    std::this_thread::sleep_for(std::chrono::seconds(3600));
}


int main() {
    // kiểm tra hệ thống có đủ điều kiện ko
    disruptor::Util::require_for_system_run_stable();


    run_single_sequencer();

    return 0;
}
