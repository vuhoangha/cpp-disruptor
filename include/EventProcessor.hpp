#pragma once

namespace disruptor {
    class EventProcessor {
    public:
        virtual ~EventProcessor() = default;

        // lấy sequence gần nhất đã xử lý
        [[nodiscard]] virtual int64_t getSequence() const = 0;

        // stop processor
        virtual void halt() = 0;
    };
}
