cmake_minimum_required(VERSION 3.28)

project(aio)
cmake_policy(SET CMP0167 NEW)
set(CMAKE_CXX_STANDARD 23)

# Apply AddressSanitizer options only in Debug mode
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    add_compile_options(-fsanitize=address -g)
    add_link_options(-fsanitize=address)
endif ()

#if (DEFINED ENV{VCPKG_ROOT})
#    set(VCPKG_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
#    if (NOT EXISTS ${VCPKG_TOOLCHAIN_FILE})
#        message(FATAL_ERROR "The vcpkg toolchain file doesn't exist at: ${VCPKG_TOOLCHAIN_FILE}")
#    endif ()
#    set(CMAKE_TOOLCHAIN_FILE ${VCPKG_TOOLCHAIN_FILE} CACHE STRING "Vcpkg toolchain file")
#else ()
#    message(FATAL_ERROR "VCPKG_ROOT environment variable is not defined. Please set it to your vcpkg installation path.")
#endif ()

# python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(async_simple CONFIG REQUIRED)
find_package(benchmark CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(fmt CONFIG REQUIRED)

# Check for liburing
pkg_check_modules(LIBURING REQUIRED liburing)

include_directories(${CMAKE_SOURCE_DIR}/io_context ${CMAKE_SOURCE_DIR}/demo/server)

# Set include directories and library paths from pkg-config
include_directories(${LIBURING_INCLUDE_DIRS})
link_directories(${LIBURING_LIBRARY_DIRS})
add_definitions(${LIBURING_CFLAGS_OTHER})

add_library(io_uring_ctx io_context/io_uring_ctx.h io_context/io_uring_ctx.cpp)
target_link_libraries(io_uring_ctx PRIVATE ${LIBURING_LIBRARIES} async_simple::async_simple_static fmt::fmt spdlog::spdlog)

add_library(tcp_server demos/server/tcp_server.h demos/server/tcp_server.cpp)
target_link_libraries(tcp_server PRIVATE io_uring_ctx ${LIBURING_LIBRARIES} fmt::fmt async_simple::async_simple_static)

add_executable(tcp_demo demos/server/main.cpp)
target_link_libraries(tcp_demo PRIVATE tcp_server io_uring_ctx async_simple::async_simple_static fmt::fmt spdlog::spdlog)

add_executable(char_count_demo demos/char_count.cpp)
target_link_libraries(char_count_demo PRIVATE io_uring_ctx async_simple::async_simple_static fmt::fmt spdlog::spdlog)

# benchmark executables
add_executable(bench_tcp_client benchmark/tcp_client.cpp)
#target_link_libraries(bench_tcp_client PRIVATE tcp_server io_uring_ctx async_simple::async_simple_static fmt::fmt spdlog::spdlog)
#target_link_libraries(HelloWorld PRIVATE ${LIBURING_LIBRARIES} fmt::fmt)

#add_executable(follow follo.cpp)
#target_link_libraries(follow PRIVATE Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util)
# target_link_libraries(HelloWorld PRIVATE ${LIBURING_LIBRARIES} fmt::fmt)
# target_link_libraries(main PRIVATE benchmark::benchmark benchmark::benchmark_main)
# target_link_libraries(main PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
# target_link_libraries(main PRIVATE async_simple::async_simple)
# target_link_libraries(network PRIVATE ${LIBURING_LIBRARIES})