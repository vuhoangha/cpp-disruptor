cmake_minimum_required(VERSION 3.10)
project(cpp-disruptor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for Release build
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g3 -ggdb")

# Google test
include(FetchContent)
FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.11.0
)
FetchContent_MakeAvailable(googletest)
# Enable testing
enable_testing()

# Add source files
set(SOURCES
    src/main.cpp
)

# Add executable
add_executable(main ${SOURCES})

# Include directories
target_include_directories(main PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
)

# Test sources
set(TEST_SOURCES
        tests/disruptor_test.cpp
)

# Add test executable
add_executable(run_tests ${TEST_SOURCES})

# Link Google Test
target_link_libraries(run_tests PRIVATE
        gtest
        gtest_main
)

# Include directories for tests
target_include_directories(run_tests PRIVATE
        ${CMAKE_SOURCE_DIR}/include
)

# Add tests
add_test(NAME AllTests COMMAND run_tests)

# Enable link-time optimization for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if(ipo_supported)
        set_property(TARGET main PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()
