cmake_minimum_required(VERSION 3.28)

project(aio)
cmake_policy(SET CMP0167 NEW)
set(CMAKE_CXX_STANDARD 23)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_TOOLCHAIN_FILE $ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake)

# set include directories from this CMake file
include_directories(${CMAKE_SOURCE_DIR})

find_package(PkgConfig REQUIRED)

# for internal io_context library
add_subdirectory(io_context)
# for network library
add_subdirectory(network)
# for demos
add_subdirectory(demos)

# python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)


# TODO: keep them here for now. I do not want to look for how to use them in the future
# find_package(spdlog CONFIG REQUIRED)
# find_package(async_simple CONFIG REQUIRED)
# find_package(benchmark CONFIG REQUIRED)
# find_package(GTest CONFIG REQUIRED)
# find_package(fmt CONFIG REQUIRED)


#add_executable(follow follo.cpp)
#target_link_libraries(follow PRIVATE Folly::folly Folly::folly_deps Folly::follybenchmark Folly::folly_test_util)
# target_link_libraries(HelloWorld PRIVATE ${LIBURING_LIBRARIES} fmt::fmt)
# target_link_libraries(main PRIVATE benchmark::benchmark benchmark::benchmark_main)
# target_link_libraries(main PRIVATE GTest::gtest GTest::gtest_main GTest::gmock GTest::gmock_main)
# target_link_libraries(main PRIVATE async_simple::async_simple)
# target_link_libraries(network PRIVATE ${LIBURING_LIBRARIES})