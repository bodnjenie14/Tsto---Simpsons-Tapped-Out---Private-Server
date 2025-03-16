#include "std_include.hpp"
#include "networking/server_startup.hpp"
#include "debugging/console.hpp"
#include "debugging/serverlog.hpp"
#include <iostream>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#endif

int main(int argc, char** argv)
{
    try
    {
        // Initialize logging with evpp's bundled glog first
        google::InitGoogleLogging(argv[0]); // disable evpp verbose logging

#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed. Error: " << WSAGetLastError() << std::endl;
            return 1;
        }
#endif
        debugging::create_console();  // Launch debug console
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_INITIALIZER, "TSTO Server Console Started");
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Starting server...");

        // Initialize and start all servers
        server::initialize_servers();  // This function contains its own event loop

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Server shutting down...");

#ifdef _WIN32
        WSACleanup();
#endif
        google::ShutdownGoogleLogging();
        return 0;
    }
    catch (const std::exception& ex)
    {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER, "Fatal error: %s", ex.what());
        std::cerr << "Fatal error: " << ex.what() << std::endl;
#ifdef _WIN32
        WSACleanup();
#endif
        google::ShutdownGoogleLogging();
        return 1;
    }
}
