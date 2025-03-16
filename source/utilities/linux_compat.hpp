#pragma once

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

namespace utils {
namespace network {

#ifdef _WIN32
    using socket_t = SOCKET;
    using socklen_t = int;
    #define SOCKET_ERROR (-1)
    #define INVALID_SOCKET (SOCKET)(~0)
    #define close_socket closesocket
#else
    using socket_t = int;
    using socklen_t = socklen_t;
    #define SOCKET_ERROR (-1)
    #define close_socket close
#endif

inline bool init_networking() {
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(2, 2), &wsa_data) == 0;
#else
    return true;
#endif
}

inline void cleanup_networking() {
#ifdef _WIN32
    WSACleanup();
#endif
}

inline int get_last_error() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}

} // namespace network
} // namespace utils
