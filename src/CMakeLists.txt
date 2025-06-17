# Danh sách các file nguồn
set(SOURCES
    main.cpp
)

# Tạo executable
add_executable(main ${SOURCES})

# Include các thư mục cần thiết
target_include_directories(main PRIVATE ${DISRUPTOR_INCLUDE_DIRS})
