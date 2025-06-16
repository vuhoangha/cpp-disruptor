#pragma once

#include <thread>
#include <chrono>
#include <string>
#include "WaitStrategy.hpp"
#include "../barriers/SequenceBarrier.hpp"

namespace disruptor {
    /**
     * Sleeping strategy that initially spins, then uses a Thread.yield(), and
     * eventually sleep (std::this_thread::sleep_for) for the minimum
     * number of nanos the OS and JVM will allow while the
     * EventProcessors are waiting on a barrier.
     *
     * This strategy is a good compromise between performance and CPU resource.
     * Latency spikes can occur after quiet periods. It will also reduce the impact
     * on the producing thread as it will not need signal any conditional variables
     * to wake up the event handling thread.
     */
    template<size_t NUMBER_DEPENDENT_SEQUENCES>
    class SleepingWaitStrategy final : public WaitStrategy<NUMBER_DEPENDENT_SEQUENCES> {
        static constexpr int SPIN_THRESHOLD = 100;
        static constexpr int DEFAULT_RETRIES = 200;
        static constexpr size_t DEFAULT_SLEEP = 100;

        const int retries;
        const size_t sleep_time_ns;

        [[nodiscard]] int apply_wait_method(const SequenceBarrier &barrier, const int counter) const {
            barrier.check_alert();

            if (counter > SPIN_THRESHOLD) {
                return counter - 1;
            }
            if (counter > 0) {
                std::this_thread::yield();
                return counter - 1;
            }
            std::this_thread::sleep_for(std::chrono::nanoseconds(sleep_time_ns));

            return counter;
        }

    public:
        /**
         * Provides a sleeping wait strategy with the default retry and sleep settings
         */
        SleepingWaitStrategy() : SleepingWaitStrategy(DEFAULT_RETRIES, DEFAULT_SLEEP) {
        }


        /**
         * @param retries How many times the strategy should retry before sleeping
         */
        explicit SleepingWaitStrategy(const int retries) : SleepingWaitStrategy(retries, DEFAULT_SLEEP) {
        }


        /**
         * @param retries How many times the strategy should retry before sleeping
         * @param sleep_time_ns How long the strategy should sleep, in nanoseconds
         */
        SleepingWaitStrategy(const int retries, const size_t sleep_time_ns) : retries(retries), sleep_time_ns(sleep_time_ns) {
        }


        [[nodiscard]] size_t wait_for(const size_t sequence,
                                      SequenceGroupForSingleThread<NUMBER_DEPENDENT_SEQUENCES> &dependent_sequences,
                                      const SequenceBarrier &barrier) override {
            size_t available_sequence;
            int counter = retries;

            while ((available_sequence = dependent_sequences.get()) < sequence) {
                counter = apply_wait_method(barrier, counter);
            }

            return available_sequence;
        }


        [[nodiscard]] std::string to_string() const noexcept override {
            return "SleepingWaitStrategy";
        }
    };
} // namespace disruptor
