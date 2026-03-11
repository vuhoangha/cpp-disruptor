#pragma once

/**
 * @file ProcessingSequenceBarrier.hpp
 * @brief Coordinates between producers and consumers via a pluggable wait strategy.
 *
 * Each BatchEventProcessor has its own barrier instance. The barrier:
 *   1. Waits for new events using the configured wait strategy (Adaptive/Yield/BusySpin)
 *   2. Handles the alert mechanism for graceful shutdown (halt signal)
 *   3. For multi-producer, delegates to sequencer.get_highest_published_sequence()
 *      to find the highest contiguous published sequence (since multi-producer
 *      can publish out of order)
 *
 * The `direct_publisher_event_listener` flag distinguishes:
 *   - true:  consumer listens directly to the producer's cursor → must check for
 *            gaps in multi-producer via get_highest_published_sequence()
 *   - false: consumer listens to another consumer's sequence → no gaps possible
 *
 * @tparam T                          WaitStrategyType enum value.
 * @tparam NUMBER_DEPENDENT_SEQUENCES Number of sequences this barrier depends on.
 * @tparam SequencerType              SingleProducerSequencer or MultiProducerSequencer.
 */

#include <thread>
#include <cassert>
#include "../sequence/Sequence.hpp"
#include "../exception/AlertException.hpp"
#include "../wait_strategy/WaitStrategyType.hpp"
#include "../wait_strategy/AdaptiveWaitStrategy.hpp"
#include "../sequence/SequenceGroupForSingleThread.hpp"
#include "../wait_strategy/YieldingWaitStrategy.hpp"
#include "../wait_strategy/BusySpinWaitStrategy.hpp"

namespace disruptor {

    /// Compile-time mapping from WaitStrategyType enum → concrete strategy class.
    template<WaitStrategyType T, size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector;

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector<WaitStrategyType::ADAPTIVE, NUMBER_DEPENDENT_SEQUENCES> {
        using type = AdaptiveWaitStrategy<NUMBER_DEPENDENT_SEQUENCES>;
    };

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector<WaitStrategyType::YIELD, NUMBER_DEPENDENT_SEQUENCES> {
        using type = YieldingWaitStrategy<NUMBER_DEPENDENT_SEQUENCES>;
    };

    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    struct WaitStrategySelector<WaitStrategyType::BUSY_SPIN, NUMBER_DEPENDENT_SEQUENCES> {
        using type = BusySpinWaitStrategy<NUMBER_DEPENDENT_SEQUENCES>;
    };

    template<WaitStrategyType T, size_t NUMBER_DEPENDENT_SEQUENCES, typename SequencerType>
    class ProcessingSequenceBarrier final {
        alignas(CACHE_LINE_SIZE) const char padding_1[CACHE_LINE_SIZE] = {};
        using Strategy = typename WaitStrategySelector<T, NUMBER_DEPENDENT_SEQUENCES>::type;
        Strategy wait_strategy;
        const bool direct_publisher_event_listener;
        const char padding_2[CACHE_LINE_SIZE * 2] = {};

        alignas(CACHE_LINE_SIZE) SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> dependent_sequences;

        alignas(CACHE_LINE_SIZE) const char padding_3[CACHE_LINE_SIZE] = {};
        bool alerted;
        const char padding_4[CACHE_LINE_SIZE - sizeof(bool)] = {};
        const char padding_5[CACHE_LINE_SIZE] = {};

        SequencerType &sequencer;

#ifndef NDEBUG
        std::thread::id owner_thread_id_{};
        bool owner_set_{false};

        bool same_thread() {
            if (!owner_set_) {
                owner_thread_id_ = std::this_thread::get_id();
                owner_set_ = true;
                return true;
            }
            return owner_thread_id_ == std::this_thread::get_id();
        }
#endif

    public:
        ProcessingSequenceBarrier(
            const bool direct_publisher_event_listener,
            std::initializer_list<std::reference_wrapper<Sequence> > dependent_sequences,
            SequencerType &sequencer)
            : direct_publisher_event_listener(direct_publisher_event_listener),
              dependent_sequences(dependent_sequences),
              alerted(false),
              sequencer(sequencer) {}

        /**
         * @brief Wait until the given sequence is available for processing.
         *
         * @param sequence The sequence number the consumer wants to process.
         * @return The highest available sequence, or SEQUENCE_ALERT if halted.
         *
         * For multi-producer with direct_publisher_event_listener=true, the returned
         * sequence may be less than what the wait strategy reported, because
         * get_highest_published_sequence() finds the highest contiguous range
         * (there may be gaps from out-of-order publishing).
         */
        [[gnu::hot]] size_t wait_for(size_t sequence) noexcept {
            assert(same_thread() && "Accessed by two threads");
            if (alerted) [[unlikely]] return SEQUENCE_ALERT;

            const size_t available_sequence = wait_strategy.wait_for(sequence, dependent_sequences, *this);
            if (available_sequence == SEQUENCE_ALERT) [[unlikely]] return SEQUENCE_ALERT;
            if (available_sequence < sequence) [[unlikely]] return available_sequence;

            if (direct_publisher_event_listener) {
                return sequencer.get_highest_published_sequence(sequence, available_sequence);
            }

            return available_sequence;
        }

        [[nodiscard]] bool is_alerted() const noexcept {
            return alerted;
        }

        /// Signal this barrier to stop — consumer will see SEQUENCE_ALERT on next wait.
        void alert() {
            alerted = true;
        }

        void clear_alert() {
            alerted = false;
        }

        [[gnu::hot]] void check_alert() const {
            if (alerted) [[unlikely]] {
                throw AlertException();
            }
        }
    };

    // Deduction guide
    template<WaitStrategyType T, size_t N, typename S>
    ProcessingSequenceBarrier(bool, std::initializer_list<std::reference_wrapper<Sequence>>, S &)
        -> ProcessingSequenceBarrier<T, N, S>;

} // namespace disruptor
