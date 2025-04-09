#pragma once
#include <exception>
#include <string>

namespace disruptor
{

    class InsufficientCapacityException : public std::exception
    {
    private:
        std::string message;

    public:
        InsufficientCapacityException() : message("Insufficient capacity in the ring buffer") {}
        explicit InsufficientCapacityException(const std::string &customMessage) : message(customMessage) {}

        const char *what() const noexcept override
        {
            return message.c_str();
        }
    };

}
