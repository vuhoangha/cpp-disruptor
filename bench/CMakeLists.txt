add_executable(benchmark benchmark.cpp)
target_include_directories(benchmark PRIVATE ${DISRUPTOR_INCLUDE_DIRS})
