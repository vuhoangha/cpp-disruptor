#pragma once
#include <functional>
#include <iostream>

#include "../sequence/Sequence.hpp"
#include "../barriers/SequenceBarrier.hpp"
#include "../ring_buffer/RingBuffer.hpp"
#include "../common/Util.hpp"

namespace disruptor {
    template<typename T, size_t BUFFER_SIZE>
    class BatchEventProcessor final {
        Sequence sequence;
        SequenceBarrier &sequence_barrier;

        using EventHandler = std::function<void(T &, size_t, bool)>;
        EventHandler event_handler;

        RingBuffer<T, BUFFER_SIZE> &ring_buffer;

    public:
        explicit BatchEventProcessor(SequenceBarrier &barrier, EventHandler handler, RingBuffer<T, BUFFER_SIZE> &ring_buffer_ptr
        ) : sequence(Util::calculate_initial_value_sequence(ring_buffer_ptr.get_buffer_size())),
            sequence_barrier(barrier),
            event_handler(handler),
            ring_buffer(ring_buffer_ptr) {
        }


        [[nodiscard]] Sequence &get_cursor() {
            return sequence;
        }


        // stop processor --> sequence barrier --> wait strategy
        void halt() const {
            sequence_barrier.alert();
        }


        void run() {
            sequence_barrier.clear_alert();
            process_events();
        }
        

        void process_events() {
            size_t next_sequence = sequence.get_relaxxxx() + 1;
            int wait_counter = 0;

            while (true) {
                try {
                    const size_t available_sequence = sequence_barrier.wait_for(next_sequence);

                    // if multi_producer_sequencer, sequence was claimed but not publish --> available_sequence = next_sequence - 1
                    if (available_sequence < next_sequence) {
                        Util::adaptive_wait(wait_counter);
                        continue;
                    }

                    while (next_sequence <= available_sequence) {
                        T &event = ring_buffer.get(next_sequence);
                        event_handler(event, next_sequence, next_sequence == available_sequence);
                        next_sequence++;
                    }

                    sequence.set_with_release(available_sequence);
                } catch (const std::exception &e) {
                    std::cout << "BatchEventProcessor exception caught: " << e.what() << std::endl;
                    break;
                }
            }
        }
    };
}
