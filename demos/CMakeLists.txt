find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(async_simple CONFIG REQUIRED)

add_executable(tcp_demo server/main.cpp)
target_link_libraries(tcp_demo PRIVATE tcp_server io_context_lib async_simple::async_simple_static fmt::fmt spdlog::spdlog)

add_executable(char_count_demo char_count.cpp)
target_link_libraries(char_count_demo PRIVATE io_context_lib async_simple::async_simple_static fmt::fmt spdlog::spdlog)

add_executable(srv201 srv.cpp)
target_link_libraries(srv201 PRIVATE io_context_lib network_lib async_simple::async_simple_static fmt::fmt spdlog::spdlog)

add_library(tcp_server server/tcp_server.h server/tcp_server.cpp)
target_link_libraries(tcp_server PRIVATE io_context_lib fmt::fmt async_simple::async_simple_static)