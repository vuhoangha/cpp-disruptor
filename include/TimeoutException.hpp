#pragma once
#include <exception>
#include <string>

namespace disruptor
{
    /**
     * Wait strategies may throw this Exception to inform callers that a
     * message has not been detected within a specific time window.
     * For efficiency, a single instance is provided.
     */
    class TimeoutException : public std::exception
    {
    private:
        std::string message;

    public:
        TimeoutException() : message("Operation timed out") {}
        explicit TimeoutException(const std::string &customMessage) : message(customMessage) {}

        const char *what() const noexcept override
        {
            return message.c_str();
        }
    };

}