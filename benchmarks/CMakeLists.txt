find_package(benchmark CONFIG REQUIRED)

add_executable(buffer_copy_bench core/buffer_copy_bench.cpp)
target_link_libraries(buffer_copy_bench PRIVATE benchmark::benchmark benchmark::benchmark_main)
