#pragma once

#include <exception>

namespace disruptor
{
    /**
     * Used to alert EventProcessors waiting at a SequenceBarrier of status changes.
     *
     * It does not fill in a stack trace for performance reasons.
     */
    class AlertException : public std::exception
    {
    public:
        /**
         * Trả về thông báo lỗi
         */
        const char *what() const noexcept override
        {
            return "Alert status changed";
        }

        /**
         * Đối tượng INSTANCE chỉ khởi tạo 1 lần
         */
        static AlertException &getInstance()
        {
            static AlertException INSTANCE;
            return INSTANCE;
        }

    private:
        /**
         * Đảm bảo ko thể tạo object từ bên ngoài
         */
        AlertException() = default;

        /**
         * Tắt chức năng copy và gán để đảm bảo singleton
         */
        AlertException(const AlertException &) = delete;
        AlertException &operator=(const AlertException &) = delete;
    };

} // namespace disruptor