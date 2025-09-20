cmake_minimum_required(VERSION 3.28)

project(aio)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(GTEST_GIT_TAG v1.17.0)

# #####################################################################################################################
# DEPENDENCIES
# #####################################################################################################################

# Gtest
include(cmake/FindGtest.cmake)

# YLT
include(cmake/FindYLT.cmake)

# Benchmark
include(cmake/FindGoogleBenchmark.cmake)

# set include directories from this CMake file
include_directories(${CMAKE_SOURCE_DIR})

add_subdirectory(core)

find_package(PkgConfig REQUIRED)

# for internal io library
add_subdirectory(io)

# for demos
add_subdirectory(demos)

# for benchmarks
add_subdirectory(benchmarks)

# python
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

# for tests
include(CTest)
enable_testing()
#add_subdirectory(tests)
