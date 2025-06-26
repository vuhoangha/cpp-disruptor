#pragma once

#include <stdexcept>
#include <cstdint>
#include <chrono>
#include <condition_variable>

namespace disruptor {
    class Util {
    public:
        [[gnu::pure]]
        static int log_2(const int value) {
            if (value < 1) {
                throw std::invalid_argument("value must be a positive number");
            }
            return 31 - __builtin_clz(value);
        }


        static int get_cache_line_size() {
#if defined(__cpp_lib_hardware_interference_size) && defined(__has_include)
#if __has_include(<new>)
#include <new>
#if defined(__cpp_lib_hardware_interference_size)
            return std::hardware_destructive_interference_size;
#endif
#endif
#endif

            // Fallback values for common architectures
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386) || defined(_M_IX86)
            return 64; // x86 & x86_64 usually have 64-byte cache lines
#elif defined(__arm__) || defined(__aarch64__) || defined(_M_ARM) || defined(_M_ARM64)
            return 64; // ARM usually has 64-byte cache lines
#elif defined(__powerpc__) || defined(__ppc__) || defined(__PPC__)
            return 128; // PowerPC often has 128-byte cache lines
#else
            return 64; // Default to 64 bytes
#endif
        }


        static void cpu_pause() noexcept {
#if defined(__x86_64__) || defined(__i386__)
            // GCC inline assembly - BEST cho Linux x86/x64
            __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__)
            // ARM64 Linux
            __asm__ __volatile__("yield" ::: "memory");
#elif defined(__arm__)
            // ARM32 Linux
            __asm__ __volatile__("yield" ::: "memory");
#else
            // Fallback
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


        // verifying that the environment is suitable for running the system
        static void require_for_system_run_stable() {
            if (get_cache_line_size() != 64) {
                std::cerr << "WARNING: CACHE_LINE not 64 bytes" << std::endl;
            }
            check_size_t_size();
            check_size_t_lock_free();
        }


        // initial the sequence with the condition that (sequence - buffer_size) must be >= 0. The reason is that the system user size_t to avoid unnecessary type casting.
        // however, size_t is always >= 0
        static size_t calculate_initial_value_sequence(const size_t buffer_size) {
            return buffer_size;
        }


        [[gnu::hot]] static void adaptive_wait(int &wait_counter) noexcept {
            static constexpr int SPIN_TRIES = 100;
            static constexpr int YIELD_TRIES = 10;
            static constexpr auto PARK_DURATION = std::chrono::nanoseconds(1);

            if (wait_counter < SPIN_TRIES) [[likely]] {
                // Phase 1: Spin-wait (no context switch)
                cpu_pause(); // x86 PAUSE instruction
                wait_counter++;
            } else if (wait_counter < SPIN_TRIES + YIELD_TRIES) [[likely]] {
                // Phase 2: Yield (light context switch)
                std::this_thread::yield();
                wait_counter++;
            } else [[unlikely]] {
                std::this_thread::sleep_for(PARK_DURATION);
                wait_counter = SPIN_TRIES;
            }
        }
    };
}
