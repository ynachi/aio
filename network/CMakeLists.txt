find_package(spdlog CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(async_simple CONFIG REQUIRED)

set(NETLIB_HEADERS base_server.h tcp_stream.h tcp_stream.h handlers.h)
set(NETLIB_SRC base_server.cpp tcp_stream.cpp handlers.cpp)
add_library(network_lib ${NETLIB_HEADERS} ${NETLIB_SRC})
target_link_libraries(network_lib PRIVATE io_context_lib fmt::fmt spdlog::spdlog async_simple::async_simple_static utils)
