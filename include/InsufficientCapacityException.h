#pragma once

#include <exception>

namespace disruptor
{

    class InsufficientCapacityException : public std::exception
    {
    public:
        /**
         * Trả về thông báo lỗi
         */
        const char *what() const noexcept override
        {
            return "Insufficient capacity in the ring buffer";
        }

        /**
         * Đối tượng INSTANCE chỉ khởi tạo 1 lần
         */
        static InsufficientCapacityException& getInstance()
        {
            static InsufficientCapacityException INSTANCE;
            return INSTANCE;
        }

    private:
        /**
         * Đảm bảo ko thể tạo object từ bên ngoài
         */
        InsufficientCapacityException() = default;

        /**
         * Tắt chức năng copy và gán để đảm bảo singleton
         */
        InsufficientCapacityException(const InsufficientCapacityException &) = delete;
        InsufficientCapacityException &operator=(const InsufficientCapacityException &) = delete;
    };

}