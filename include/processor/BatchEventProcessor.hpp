#pragma once

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


        [[gnu::hot]] void process_events() {
            size_t next_sequence = sequence.get() + 1;
            int wait_counter = 0;

            while (true) {
                const size_t available_sequence = sequence_barrier.wait_for(next_sequence);

                if (available_sequence == SEQUENCE_ALERT) [[unlikely]] break;

                // Multi-producer: sequence claimed but not yet published
                if (available_sequence < next_sequence) [[unlikely]] {
                    Util::adaptive_wait(wait_counter);
                    continue;
                }

                wait_counter = 0;

                while (next_sequence <= available_sequence) {
                    // Prefetch next ring buffer entry while processing current one
                    __builtin_prefetch(&ring_buffer.get(next_sequence + 1), 0, 3);
                    T &event = ring_buffer.get(next_sequence);
                    event_handler(event, next_sequence, next_sequence == available_sequence);
                    next_sequence++;
                }

                sequence.set_with_release(available_sequence);
            }
        }
    };

    // Deduction guide
    template<typename T, size_t BUFFER_SIZE, typename EventHandler, typename BarrierType>
    BatchEventProcessor(BarrierType &, EventHandler, RingBuffer<T, BUFFER_SIZE> &)
        -> BatchEventProcessor<T, BUFFER_SIZE, EventHandler, BarrierType>;
}
