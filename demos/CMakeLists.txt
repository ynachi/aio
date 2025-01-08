find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(async_simple CONFIG REQUIRED)

add_executable(tcp_demo low_level_tcp_server/main.cpp)
target_link_libraries(tcp_demo PRIVATE tcp_server io_context_lib async_simple::async_simple_static fmt::fmt spdlog::spdlog)

add_executable(char_count_demo char_count_low.cpp)
target_link_libraries(char_count_demo PRIVATE io_context_lib async_simple::async_simple_static fmt::fmt spdlog::spdlog)

add_executable(srv201 tcp_server_201.cpp)
target_link_libraries(srv201 PRIVATE io_context_lib network_lib async_simple::async_simple_static fmt::fmt spdlog::spdlog)

add_library(tcp_server low_level_tcp_server/tcp_server.h low_level_tcp_server/tcp_server.cpp)
target_link_libraries(tcp_server PRIVATE io_context_lib fmt::fmt async_simple::async_simple_static)