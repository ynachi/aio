find_package(async_simple CONFIG REQUIRED)

add_library(utils buffer.h buffer.cpp errors.h)
target_link_libraries(utils PRIVATE async_simple::async_simple_static)