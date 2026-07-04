cmake_minimum_required(VERSION 3.10)

project(demo-poll)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 收集src下所有源文件
file(GLOB SRC_FILES ${PROJECT_SOURCE_DIR}/src/*.cpp)

# 生成两个可执行文件
add_executable(main ${SRC_FILES})
add_executable(client client.cpp)

# 给main添加头文件目录
target_include_directories(main
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

find_package(Threads REQUIRED)
target_link_libraries(main PRIVATE Threads::Threads)