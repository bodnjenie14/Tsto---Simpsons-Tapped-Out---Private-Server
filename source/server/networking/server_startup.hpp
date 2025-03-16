#pragma once

#include <evpp/tcp_server.h>
#include <evpp/buffer.h>
#include <evpp/tcp_conn.h>
#include <evpp/udp/udp_server.h>
#include <evpp/udp/udp_message.h>
#include <evpp/http/http_server.h>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netdb.h>
#include <ifaddrs.h>
#endif

namespace server {
    std::string get_local_ipv4();
    void initialize_servers();
}
