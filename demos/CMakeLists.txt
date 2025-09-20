add_executable(tcp_demo low_level_tcp_server/main.cpp)
target_link_libraries(tcp_demo PRIVATE tcp_server io yalantinglibs::yalantinglibs)

add_executable(bench_server low_level_tcp_server/bench_server.cpp)
target_link_libraries(bench_server PRIVATE yalantinglibs::yalantinglibs)

add_executable(char_count_demo char_count_low.cpp)
target_link_libraries(char_count_demo PRIVATE io yalantinglibs::yalantinglibs)

add_executable(net_tcp_demo net_tcp_demo.cpp)
target_link_libraries(net_tcp_demo PRIVATE io network_lib yalantinglibs::yalantinglibs)


add_library(tcp_server low_level_tcp_server/tcp_server.h low_level_tcp_server/tcp_server.cpp)
target_link_libraries(tcp_server PRIVATE io yalantinglibs::yalantinglibs)