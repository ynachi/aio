find_package(GTest CONFIG REQUIRED)
find_package(async_simple CONFIG REQUIRED)

add_executable(in_memory_context_test io_context/in_memory_context_test.cpp)
target_link_libraries(in_memory_context_test GTest::gtest_main async_simple::async_simple io_context_lib)
add_test(NAME in_memory_context_test COMMAND in_memory_context_test)

add_executable(tcp_stream_test network/tcp_stream_test.cpp)
target_link_libraries(tcp_stream_test GTest::gtest_main async_simple::async_simple io_context_lib network_lib) 
add_test(NAME tcp_stream_test COMMAND tcp_stream_test)