#pragma once
#include <iostream>

#include "../sequence/Sequence.hpp"
#include "../ring_buffer/RingBuffer.hpp"
#include "../common/Util.hpp"

namespace disruptor {
    template<typename T, size_t BUFFER_SIZE, typename EventHandler, typename BarrierType>
    class BatchEventProcessor final {
        Sequence sequence;
        BarrierType &sequence_barrier;

        EventHandler event_handler;

        RingBuffer<T, BUFFER_SIZE> &ring_buffer;

    public:
        explicit BatchEventProcessor(BarrierType &barrier, EventHandler handler, RingBuffer<T, BUFFER_SIZE> &ring_buffer_ptr
        ) : sequence(Util::calculate_initial_value_sequence(ring_buffer_ptr.get_buffer_size())),
            sequence_barrier(barrier),
            event_handler(std::move(handler)),
            ring_buffer(ring_buffer_ptr) {
        }

        [[nodiscard]] Sequence &get_cursor() {
            return sequence;
        }

        void halt() const {
            sequence_barrier.alert();
        }

        void run() {
            sequence_barrier.clear_alert();
            process_events();
        }


        void process_events() {
            size_t next_sequence = sequence.get() + 1;
            int wait_counter = 0;

            while (true) {
                try {
                    const size_t available_sequence = sequence_barrier.wait_for(next_sequence);

                    // Multi-producer: sequence claimed but not yet published → available_sequence < next_sequence
                    if (available_sequence < next_sequence) {
                        Util::adaptive_wait(wait_counter);
                        continue;
                    }

                    wait_counter = 0;

                    while (next_sequence <= available_sequence) {
                        T &event = ring_buffer.get(next_sequence);
                        event_handler(event, next_sequence, next_sequence == available_sequence);
                        next_sequence++;
                    }

                    sequence.set_with_release(available_sequence);
                } catch (const std::exception &e) {
                    std::cerr << "BatchEventProcessor exception caught: " << e.what() << std::endl;
                    break;
                }
            }
        }
    };

    // Deduction guide
    template<typename T, size_t BUFFER_SIZE, typename EventHandler, typename BarrierType>
    BatchEventProcessor(BarrierType &, EventHandler, RingBuffer<T, BUFFER_SIZE> &)
        -> BatchEventProcessor<T, BUFFER_SIZE, EventHandler, BarrierType>;
}
