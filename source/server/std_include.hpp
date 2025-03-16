#pragma once

// Standard C++ includes
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>
#include <functional>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <cstdint>
#include <cstring>
#include <cctype>

// Platform-specific includes
#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <direct.h>
#include <io.h>
#else
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#endif

// Project includes
#include <tomcrypt.h>
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

// Platform-specific definitions
#ifdef _WIN32
    #define DIRECTORY_SEPARATOR "\\"
    #define PATH_SEPARATOR ";"
    #define LIBRARY_EXTENSION ".dll"
#else
    #define DIRECTORY_SEPARATOR "/"
    #define PATH_SEPARATOR ":"
    #define LIBRARY_EXTENSION ".so"
    
    // Windows types for Linux
    typedef void* HANDLE;
    typedef unsigned long DWORD;
    typedef long LONG;
    typedef int BOOL;
    typedef unsigned char BYTE;
    typedef unsigned short WORD;
    typedef const char* LPCSTR;
    typedef char* LPSTR;
    typedef void* LPVOID;
    typedef const void* LPCVOID;
    typedef size_t SIZE_T;
    
    #define TRUE 1
    #define FALSE 0
    #define INVALID_HANDLE_VALUE ((HANDLE)-1)
    #define MAX_PATH 260
    
    // Common Windows functions
    inline void Sleep(DWORD dwMilliseconds) {
        usleep(dwMilliseconds * 1000);
    }
    
    inline DWORD GetLastError() {
        return errno;
    }
    
    inline void SetLastError(DWORD dwErrCode) {
        errno = dwErrCode;
    }
#endif

// Common macros and utilities
#define SAFE_DELETE(p) { if(p) { delete (p); (p) = nullptr; } }
#define SAFE_DELETE_ARRAY(p) { if(p) { delete[] (p); (p) = nullptr; } }
#define SAFE_FREE(p) { if(p) { free(p); (p) = nullptr; } }

namespace utils
{
    inline std::string get_last_error_string()
    {
#ifdef _WIN32
        DWORD error = GetLastError();
        if (error == 0) return "";

        LPSTR buffer = nullptr;
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&buffer, 0, nullptr);

        std::string message(buffer, size);
        LocalFree(buffer);

        return message;
#else
        return std::string(strerror(errno));
#endif
    }
}

// min and max is required by gdi, therefore NOMINMAX won't work
#ifdef max
#undef max
#endif

#ifdef min
#undef min
#endif

using namespace std::literals;

extern uint32_t server_start_time;

extern void restart_server_logic();  // for server restart 

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "urlmon.lib" )
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "Crypt32.lib")
#pragma comment(lib, "Pdh.lib") //pc stats (cpu ram ect)

#pragma comment(lib, "bcrypt.lib") // libevent-arc4random
//#pragma comment(lib, "glog.lib") 
