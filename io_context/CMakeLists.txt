# Check for liburing
pkg_check_modules(LIBURING REQUIRED liburing)
include_directories(${CMAKE_SOURCE_DIR}/io_context ${CMAKE_SOURCE_DIR}/demo/server)
# Set include directories and library paths from pkg-config
include_directories(${LIBURING_INCLUDE_DIRS})
link_directories(${LIBURING_LIBRARY_DIRS})
add_definitions(${LIBURING_CFLAGS_OTHER})

add_library(io_context_lib uring_context.h io_context.h memory_stream_context.h memory_stream_context.cpp)
target_link_libraries(io_context_lib PRIVATE ${LIBURING_LIBRARIES} yalantinglibs::yalantinglibs)

