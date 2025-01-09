find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(async_simple CONFIG REQUIRED)

add_library(network_lib tcp_server.cpp tcp_server.h tcp_stream.h tcp_stream.cpp)
target_link_libraries(network_lib PRIVATE io_context_lib fmt::fmt spdlog::spdlog async_simple::async_simple_static utils)
