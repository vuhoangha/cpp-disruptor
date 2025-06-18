#pragma once

#include <string>

struct TestEvent {
    long value;
    std::string message;

    // Constructor mặc định để factory có thể tạo
    TestEvent() : value(-1), message("Initial") {}
};

inline TestEvent createTestEvent() {
    return TestEvent{};
}
