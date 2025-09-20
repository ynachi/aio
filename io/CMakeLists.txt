# Check for liburing
pkg_check_modules(LIBURING REQUIRED liburing)
include_directories(${CMAKE_SOURCE_DIR}/io ${CMAKE_SOURCE_DIR}/demo/server)

set(IO_HEADER_FILES context.h)
set(IO_SRC_FILES context.cpp)

add_library(io ${IO_HEADER_FILES} ${IO_SRC_FILES})
target_link_libraries(io PUBLIC ${LIBURING_LIBRARIES} yalantinglibs::yalantinglibs)

