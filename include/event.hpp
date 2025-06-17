#pragma once

namespace disruptor {
    class Event {
        size_t value = 0;

    public:
        Event() = default;

        void set_value(const size_t value) {
            this->value = value;
        }

        [[nodiscard]] size_t get_value() const {
            return value;
        }
    };
}
