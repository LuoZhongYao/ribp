add_executable(rscv main.c riscv-stub.c)

include_directories(${CMAKE_SOURCE_DIR}/riscv)
target_link_libraries(rscv riscv)
