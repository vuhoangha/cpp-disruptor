#pragma once

/**
 * @file AlertException.hpp
 * @brief Exception used to signal halt/shutdown to event processors.
 *
 * Note: This exception is only used for the check_alert() path (non-hot).
 * The hot path (wait_for()) uses SEQUENCE_ALERT sentinel value instead
 * to avoid the overhead of exception throwing/catching during normal operation.
 */

#include <exception>
#include <string>

namespace disruptor {

    class AlertException : public std::exception {
    private:
        std::string message;

    public:
        AlertException() : message("Alert status changed") {}
        explicit AlertException(const std::string &customMessage) : message(customMessage) {}

        const char *what() const noexcept override {
            return message.c_str();
        }
    };

} // namespace disruptor
