#pragma once

/**
 * @file Util.hpp
 * @brief Utility functions for the Disruptor++ library.
 *
 * Provides platform-specific helpers for:
 * - CPU pause/yield instructions (spin-wait optimization)
 * - Thread-to-core pinning (CPU affinity)
 * - Adaptive wait strategy (3-phase: spin → yield → sleep)
 * - Environment validation checks
 */

#include <stdexcept>
#include <cstdint>
#include <chrono>
#include <thread>
#include <iostream>

#if defined(__linux__)
#include <pthread.h>
#include <sched.h>
#endif

namespace disruptor {

    class Util {
    public:
        /**
         * @brief Fast integer log2 using compiler builtin (CLZ instruction on x86).
         * @param value Must be >= 1.
         * @return floor(log2(value))
         */
        [[gnu::pure]]
        static int log_2(const int value) {
            if (value < 1) {
                throw std::invalid_argument("value must be a positive number");
            }
            return 31 - __builtin_clz(value);
        }

        /**
         * @brief Detect cache line size at compile time based on target architecture.
         * @return Cache line size in bytes (64 for x86/ARM, 128 for PowerPC).
         */
        static int get_cache_line_size() {
#if defined(__cpp_lib_hardware_interference_size) && defined(__has_include)
#if __has_include(<new>)
#include <new>
#if defined(__cpp_lib_hardware_interference_size)
            return std::hardware_destructive_interference_size;
#endif
#endif
#endif
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
            return 64;
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
            return 64;
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
            return 128;
#else
            return 64;
#endif
        }

        /**
         * @brief Emit a CPU pause/yield hint to reduce contention in spin-wait loops.
         *
         * On x86: PAUSE instruction — delays ~40 cycles, reduces pipeline flush penalty
         * on Hyper-Threading siblings, and signals the CPU to optimize power usage.
         * On ARM: YIELD instruction — similar hint for the core scheduler.
         */
        static void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(__i386__)
            __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
            __asm__ __volatile__("yield" ::: "memory");
#else
            std::this_thread::yield();
#endif
        }

        static void check_size_t_size() {
            if constexpr (sizeof(size_t) != 8) {
                std::cerr << "WARNING: Size of size_t: " << sizeof(size_t)
                        << " bytes, not 8 bytes as expected!" << std::endl;
            }
        }

        static void check_size_t_lock_free() {
            const std::atomic<std::size_t> as;
            if (!as.is_lock_free()) {
                std::cerr << "WARNING: size_t not support lock free" << std::endl;
            }
        }

        /// Validate that the runtime environment meets requirements for lock-free operation.
        static void require_for_system_run_stable() {
            if (get_cache_line_size() != 64) {
                std::cerr << "WARNING: CACHE_LINE not 64 bytes" << std::endl;
            }
            check_size_t_size();
            check_size_t_lock_free();
        }

        /**
         * @brief Calculate the initial sequence value for sequencers.
         *
         * Sequences start at buffer_size (not 0) so that (sequence - buffer_size) is
         * always >= 0. This avoids underflow issues since we use size_t (unsigned).
         * The first real event will be at sequence = buffer_size + 1.
         */
        static size_t calculate_initial_value_sequence(const size_t buffer_size) {
            return buffer_size;
        }

        /**
         * @brief Pin the calling thread to a specific CPU core (Linux only).
         * @param core_id Logical CPU ID (e.g., 0, 1, 8, 10).
         * @return true if pinning succeeded.
         *
         * Used to eliminate OS scheduler jitter and keep hot data in the core's
         * private L1/L2 caches. For best results, pin to one thread per physical
         * core (avoid Hyper-Threading siblings sharing the same core).
         */
        static bool pin_thread_to_core(int core_id) noexcept {
#if defined(__linux__)
            cpu_set_t cpuset;
            CPU_ZERO(&cpuset);
            CPU_SET(core_id, &cpuset);
            return pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) == 0;
#else
            return false;
#endif
        }

        /**
         * @brief 3-phase adaptive wait — balances latency vs CPU usage.
         *
         * Phase 1 (0–99):   Spin with PAUSE — sub-microsecond latency, no context switch.
         * Phase 2 (100–109): Yield — give up timeslice, ~1-15μs latency.
         * Phase 3 (110+):    Sleep 1ns — minimal CPU usage, resets to phase 2.
         *
         * The reset to SPIN_TRIES (not 0) after sleep ensures we don't re-enter
         * the tight spin phase unnecessarily, keeping the wait in yield/sleep territory
         * for sustained contention.
         *
         * @param wait_counter Mutable counter tracking the current wait phase.
         *                     Caller must initialize to 0 and pass the same variable
         *                     across consecutive wait iterations.
         */
        [[gnu::hot]] static void adaptive_wait(int &wait_counter) noexcept {
            static constexpr int SPIN_TRIES = 100;
            static constexpr int YIELD_TRIES = 10;
            static constexpr auto PARK_DURATION = std::chrono::nanoseconds(1);

            if (wait_counter < SPIN_TRIES) [[likely]] {
                cpu_pause();
                wait_counter++;
            } else if (wait_counter < SPIN_TRIES + YIELD_TRIES) [[likely]] {
                std::this_thread::yield();
                wait_counter++;
            } else [[unlikely]] {
                std::this_thread::sleep_for(PARK_DURATION);
                wait_counter = SPIN_TRIES;
            }
        }
    };

} // namespace disruptor
