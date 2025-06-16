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
                std::cerr << "CẢNH BÁO: Kích thước của size_t là " << sizeof(size_t)
                        << " bytes, không phải 8 bytes như mong đợi!" << std::endl;
                std::cerr << "Điều này có thể gây ra vấn đề về hiệu suất hoặc tính đúng đắn của chương trình." << std::endl;
            }
        }


        static void check_size_t_lock_free() {
            const std::atomic<std::size_t> as;
            if (!as.is_lock_free()) {
                std::cerr << "CẢNH BÁO: size_t ko support lock free" << std::endl;
            }
        }


        // kiểm tra điều kiện môi trường có đáp ứng để chương trình chạy ổn định và hiệu quả không
        static void require_for_system_run_stable() {
            // kiểm tra cache line
            if (Util::get_cache_line_size() != 64) {
                std::cerr << "CẢNH BÁO: CACHE_LINE ko phải 64 bytes" << std::endl;
            }

            // kiểm tra kiểu size_t
            Util::check_size_t_size();

            // kiểm tra atomic size_t lockfree
            Util::check_size_t_lock_free();
        }


        // khởi tạo sequence với điều kiện sequence - buffer_size phải >= 0. Đang cố để mọi biến theo kiểu size_t tránh việc phải ép kiểu
        static size_t calculate_initial_value_sequence(const size_t buffer_size) {
            return buffer_size;
        }
    };
}
