add_executable(benchmark_tcp_libevent_client client.c)
target_link_libraries(benchmark_tcp_libevent_client libevent::core libevent::extra libevent::openssl)

add_executable(benchmark_tcp_libevent_server server.c)
target_link_libraries(benchmark_tcp_libevent_server libevent::core libevent::extra libevent::openssl)
