cmake_minimum_required(VERSION 3.10)
project(cpp-disruptor)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Optimization flags for Release build
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

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

# Enable link-time optimization for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported)
    if(ipo_supported)
        set_property(TARGET main PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()
