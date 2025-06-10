#pragma once
#include <cstdint>

namespace disruptor {
    class Event {
        size_t value = 0;

    public:
        Event() = default;

        void setValue(const size_t value) {
            this->value = value;
        }

        [[nodiscard]] size_t getValue() const {
            return this->value;
        }
    };
}
