
add_executable(benchmark_ioevent_libevent libevent_ioevent_bench.c)
target_link_libraries(benchmark_ioevent_libevent libevent::core libevent::extra libevent::openssl)
