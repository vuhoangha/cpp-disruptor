#pragma once
#include <exception>
#include <string>

namespace disruptor
{

    /**
     * Used to alert EventProcessors waiting at a SequenceBarrier of status changes.
     */
    class AlertException : public std::exception
    {
    private:
        std::string message;

    public:
        AlertException() : message("Alert status changed") {}
        explicit AlertException(const std::string &customMessage) : message(customMessage) {}

        const char *what() const noexcept override
        {
            return message.c_str();
        }
    };

}
