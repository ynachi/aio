add_library(network_lib INTERFACE)
target_sources(io INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/tcp.h>)
target_link_libraries(network_lib INTERFACE io yalantinglibs::yalantinglibs utils)
