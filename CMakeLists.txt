
cmake_minimum_required(VERSION 3.10)
project(cpp-disruptor)

# Thiết lập phiên bản C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Tạo biến để kiểm soát việc build tests
option(BUILD_TESTING "Build the testing tree." ON)

# Optimization flags nâng cao cho build Release
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -mtune=native -DNDEBUG")

# Debug flags
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -O0 -g3 -ggdb -DNDEBUG")

# Thiết lập các biến chung cho toàn dự án
set(DISRUPTOR_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/barriers
    ${CMAKE_SOURCE_DIR}/include/common
    ${CMAKE_SOURCE_DIR}/include/exception
    ${CMAKE_SOURCE_DIR}/include/processor
    ${CMAKE_SOURCE_DIR}/include/ring_buffer
    ${CMAKE_SOURCE_DIR}/include/sequence
    ${CMAKE_SOURCE_DIR}/include/sequencer
    ${CMAKE_SOURCE_DIR}/include/wait_strategy
)

# Thêm các thư mục con
add_subdirectory(src)

# Thiết lập Google Test chỉ khi BUILD_TESTING=ON
if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.11.0
    )
    FetchContent_MakeAvailable(googletest)
    enable_testing()
    add_subdirectory(tests)
endif()

# Nếu có thư viện riêng trong tương lai, thêm ở đây
# add_subdirectory(lib)

# Enable link-time optimization cho build Release
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if(ipo_supported)
        message(STATUS "IPO / LTO enabled for Release build")
        set_property(TARGET main PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(STATUS "IPO / LTO not supported")
    endif()
endif()