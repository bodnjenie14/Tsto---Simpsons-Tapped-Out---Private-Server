
add_executable(benchmark_http_libevent libevent_http_bench.c)
target_link_libraries(benchmark_http_libevent libevent::core libevent::extra libevent::openssl)
