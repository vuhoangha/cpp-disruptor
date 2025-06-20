# Thư mục gốc của tests
set(TEST_ROOT ${CMAKE_SOURCE_DIR}/tests)

# Unit tests
file(GLOB_RECURSE UNIT_TEST_SOURCES
        ${TEST_ROOT}/unit/**/*_test.cpp
)

# Integration tests
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES
        ${TEST_ROOT}/integration/*_test.cpp
)

# Performance tests
file(GLOB_RECURSE PERFORMANCE_TEST_SOURCES
        ${TEST_ROOT}/performance/*_test.cpp
)

# Stress tests
file(GLOB_RECURSE STRESS_TEST_SOURCES
        ${TEST_ROOT}/stress/*_test.cpp
)

# Tất cả các tests
set(ALL_TEST_SOURCES
        ${TEST_ROOT}/main_test.cpp
        ${UNIT_TEST_SOURCES}
        ${INTEGRATION_TEST_SOURCES}
        ${PERFORMANCE_TEST_SOURCES}
        ${STRESS_TEST_SOURCES}
)

# Tạo target cho tất cả các tests
add_executable(run_all_tests ${ALL_TEST_SOURCES})
target_link_libraries(run_all_tests PRIVATE
        gmock_main
)
# Include các thư mục cần thiết
target_include_directories(run_all_tests PRIVATE
        ${DISRUPTOR_INCLUDE_DIRS}
        ${TEST_ROOT}/common
)

# Tạo target riêng cho unit tests
add_executable(run_unit_tests
        ${TEST_ROOT}/main_test.cpp
        ${UNIT_TEST_SOURCES}
)
target_link_libraries(run_unit_tests PRIVATE
        gmock_main
)
# Include các thư mục cần thiết
target_include_directories(run_unit_tests PRIVATE
        ${DISRUPTOR_INCLUDE_DIRS}
        ${TEST_ROOT}/common
)

# Tạo target riêng cho performance tests
add_executable(run_performance_tests
        ${TEST_ROOT}/main_test.cpp
        ${PERFORMANCE_TEST_SOURCES}
)
target_link_libraries(run_performance_tests PRIVATE
        gmock_main
)
# Include các thư mục cần thiết
target_include_directories(run_performance_tests PRIVATE
        ${DISRUPTOR_INCLUDE_DIRS}
        ${TEST_ROOT}/common
)

# Đăng ký các tests với CTest
add_test(NAME AllTests COMMAND run_all_tests)
add_test(NAME UnitTests COMMAND run_unit_tests)
add_test(NAME PerformanceTests COMMAND run_performance_tests)
