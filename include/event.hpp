#pragma once
#include <cstdint>

namespace disruptor {
    class Event {
        int64_t value = 0;

    public:
        Event() = default;

        void setValue(const int64_t value) {
            this->value = value;
        }

        [[nodiscard]] int64_t getValue() const {
            return this->value;
        }
    };
}
