#pragma once

#include <stdexcept>
#include <cstdint>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace disruptor {
    class Util {
    public:
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
    };
}
