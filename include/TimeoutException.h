#pragma once

#include <exception>

namespace disruptor
{
    /**
     * Wait strategies may throw this Exception to inform callers that a
     * message has not been detected within a specific time window.
     * For efficiency, a single instance is provided.
     */
    class TimeoutException : public std::exception
    {
    public:
        /**
         * Trả về thông báo lỗi
         */
        const char *what() const noexcept override
        {
            return "Operation timed out";
        }

        /**
         * Đối tượng INSTANCE chỉ khởi tạo 1 lần
         */
        static TimeoutException &getInstance()
        {
            static TimeoutException INSTANCE;
            return INSTANCE;
        }

    private:
        /**
         * Đảm bảo ko thể tạo object từ bên ngoài
         */
        TimeoutException() = default;

        /**
         * Tắt chức năng copy và gán để đảm bảo singleton
         */
        TimeoutException(const TimeoutException &) = delete;
        TimeoutException &operator=(const TimeoutException &) = delete;
    };

} // namespace disruptor