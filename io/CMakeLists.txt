# Check for liburing
pkg_check_modules(LIBURING REQUIRED liburing)
include_directories(${CMAKE_SOURCE_DIR}/io ${CMAKE_SOURCE_DIR}/demo/server)

add_library(io_context_lib INTERFACE)
target_sources(io_context_lib INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/uring_context.h>)
target_link_libraries(io_context_lib INTERFACE ${LIBURING_LIBRARIES} yalantinglibs::yalantinglibs)

