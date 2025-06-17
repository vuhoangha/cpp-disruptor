#pragma once

#include <unordered_map>
#include "SequenceBarrier.hpp"
#include "../sequence/Sequence.hpp"
#include "../sequencer/Sequencer.hpp"
#include "../exception/AlertException.hpp"
#include "../wait_strategy/WaitStrategyType.hpp"
#include "../wait_strategy/BusySpinWaitStrategy.hpp"
#include "../sequence/SequenceGroupForSingleThread.hpp"
#include "../wait_strategy/SleepingWaitStrategy.hpp"
#include "../wait_strategy/YieldingWaitStrategy.hpp"

/**
 * each processor will have a single corresponding sequence barrier. The purpose is to optimize cache
 * values within sequence barrier for each processor and to avoid race conditions.
 */
namespace disruptor {
    template<WaitStrategyType T, size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector;

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> {
        using type = BusySpinWaitStrategy<NUMBER_DEPENDENT_SEQUENCES>;
    };

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector<WaitStrategyType::SLEEP, NUMBER_DEPENDENT_SEQUENCES> {
        using type = SleepingWaitStrategy<NUMBER_DEPENDENT_SEQUENCES>;
    };

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector<WaitStrategyType::YIELD, NUMBER_DEPENDENT_SEQUENCES> {
        using type = YieldingWaitStrategy<NUMBER_DEPENDENT_SEQUENCES>;
    };

    template<WaitStrategyType T, size_t NUMBER_DEPENDENT_SEQUENCES>
    class ProcessingSequenceBarrier final : public SequenceBarrier {
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        using Strategy = typename WaitStrategySelector<T, NUMBER_DEPENDENT_SEQUENCES>::type;
        Strategy wait_strategy;
        const bool direct_publisher_event_listener;     // listen directly to events from the publisher, not from any dependent processor
        const char padding_2[CACHE_LINE_SIZE * 2] = {};

        alignas(CACHE_LINE_SIZE) SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> dependent_sequences;

        alignas(CACHE_LINE_SIZE) const char padding_3[CACHE_LINE_SIZE] = {};
        bool alerted;
        const char padding_4[CACHE_LINE_SIZE - sizeof(bool)] = {};
        const char padding_5[CACHE_LINE_SIZE] = {};

        Sequencer &sequencer;

        // allow single thread access to the sequence barrier
        bool same_thread() {
            return SequenceBarrierThreadAssertion::is_same_thread(this);
        }

    public:
        ProcessingSequenceBarrier(
            const bool direct_publisher_event_listener,
            std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences,
            Sequencer &sequencer)
            : direct_publisher_event_listener(direct_publisher_event_listener),
              dependent_sequences(dependent_sequences),
              alerted(false),
              sequencer(sequencer) {
        }

        // wait for a specific sequence to be ready for processing
        size_t wait_for(size_t sequence) override {
            assert(same_thread() && "Accessed by two threads");
            check_alert();

            const size_t available_sequence = wait_strategy.wait_for(sequence, dependent_sequences, *this);
            if (available_sequence < sequence) {
                return available_sequence;
            }

            if (direct_publisher_event_listener) {
                // listen directly from the publisher
                // single producer: directly returns "available_sequence"
                // multi producer: returns the highest contiguous sequence that has been published
                return sequencer.get_highest_published_sequence(sequence, available_sequence);
            }

            // wait after other processors
            // this sequence is guaranteed to have been published
            return available_sequence;
        }

        [[nodiscard]] bool is_alerted() const override {
            return alerted;
        }

        void alert() override {
            alerted = true;
        }

        void clear_alert() override {
            alerted = false;
        }

        void check_alert() const override {
            if (alerted) {
                throw AlertException();
            }
        }

        /**
         * Only used when assertions are enabled.
         */
        class SequenceBarrierThreadAssertion {
            static inline std::unordered_map<ProcessingSequenceBarrier *, std::thread::id> SEQUENCE_BARRIERS;
            static inline std::mutex producers_mutex;

        public:
            static bool is_same_thread(ProcessingSequenceBarrier *sequence_barrier) {
                std::lock_guard lock(producers_mutex);

                const std::thread::id current_thread = std::this_thread::get_id();
                if (!SEQUENCE_BARRIERS.contains(sequence_barrier)) {
                    SEQUENCE_BARRIERS[sequence_barrier] = current_thread;
                }

                return SEQUENCE_BARRIERS[sequence_barrier] == current_thread;
            }
        };
    };
}
