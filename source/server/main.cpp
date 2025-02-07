#include <std_include.hpp>

#include "networking/server_startup.hpp"
#include "debugging/console.hpp"
#include "debugging/blackbox.hpp"
#include "debugging/serverlog.hpp"
#include <iostream>

#include <tsto_server.hpp>

namespace tsto {

    bool TSTOServer::initialize(uint16_t port) {
        server_.SetThreadDispatchPolicy(evpp::ThreadDispatchPolicy::kIPAddressHashing);

        if (!server_.Init({ port })) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Failed to initialize TSTO server on port %d", port);
            return false;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
            "TSTO server initialized on port %d", port);
        return true;
    }

    void TSTOServer::start() {
        server_.Start();
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "TSTO server started");
    }

    void TSTOServer::stop() {
        server_.Stop();
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "TSTO server stopped");
    }
}

    int main(int argc, char* argv[]) {
  
        google::InitGoogleLogging(argv[0]); // disable evpp verbose logging

        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed. Error: " << WSAGetLastError() << std::endl;
            return 1;
        }

        create_console();  // Launch debug console
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Starting server...");

        blackbox::initialize_exception_handler();
        initialize_servers();

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Server shutting down...");

        WSACleanup();
        google::ShutdownGoogleLogging();

        return 0;
    }
