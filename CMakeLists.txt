project(riscv)
cmake_minimum_required(VERSION 3.8)
set(CMAKE_C_FLAGS "-g")

include_directories(include)

add_subdirectory(src)
add_subdirectory(riscv)

