set(NETLIB_HEADERS base_server.h tcp_server.h stream.h tcp.h)
set(NETLIB_SRC base_server.cpp stream.cpp)
add_library(network_lib ${NETLIB_HEADERS} ${NETLIB_SRC})
target_link_libraries(network_lib PRIVATE io_context_lib yalantinglibs::yalantinglibs utils)
