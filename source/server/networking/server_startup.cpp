#include <std_include.hpp>
#include "server_startup.hpp"
#include "dispatcher/dispatcher.hpp"
#include "debugging/console.hpp"
#include "debugging/serverlog.hpp"
#include "configuration.hpp"
#include <WS2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

std::string get_local_ipv4() {
    ULONG bufferSize = 0;
    if (GetAdaptersAddresses(AF_INET, 0, nullptr, nullptr, &bufferSize) == ERROR_BUFFER_OVERFLOW) {
        std::vector<unsigned char> buffer(bufferSize);
        auto addresses = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
        
        if (GetAdaptersAddresses(AF_INET, 0, nullptr, addresses, &bufferSize) == ERROR_SUCCESS) {
            for (auto adapter = addresses; adapter != nullptr; adapter = adapter->Next) {
                // Skip loopback and disabled adapters
                if (adapter->OperStatus != IfOperStatusUp || 
                    adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                    continue;

                auto address = adapter->FirstUnicastAddress;
                while (address != nullptr) {
                    if (address->Address.lpSockaddr->sa_family == AF_INET) {
                        char ip[INET_ADDRSTRLEN];
                        sockaddr_in* sockaddr = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
                        inet_ntop(AF_INET, &(sockaddr->sin_addr), ip, INET_ADDRSTRLEN);
                        return std::string(ip);
                    }
                    address = address->Next;
                }
            }
        }
    }
    return "127.0.0.1"; // Fallback to localhost
}

void initialize_servers() {  //for now dlc on same port as game
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Initializing HTTP Servers...");
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Version 0.04");

    const char* CONFIG_SECTION = "ServerConfig";
    std::string detected_ip = get_local_ipv4();
    std::string server_ip = utils::configuration::ReadString(CONFIG_SECTION, "ServerIP", detected_ip);
    int game_port = static_cast<int>(utils::configuration::ReadUnsignedInteger(CONFIG_SECTION, "GamePort", 80));
    //int dlc_port = static_cast<int>(utils::configuration::ReadUnsignedInteger(CONFIG_SECTION, "DLCPort", 3074));

    auto tsto_server = std::make_shared<tsto::TSTOServer>();
    tsto_server->server_ip_ = server_ip;
    tsto_server->server_port_ = static_cast<uint16_t>(game_port);
    
    auto dispatcher = std::make_shared<server::dispatcher::http::Dispatcher>(tsto_server);

    //// DLC Server on port 3074
    //evpp::EventLoop dlc_loop;
    //evpp::http::Server dlc_server(2);
    //dlc_server.SetThreadDispatchPolicy(evpp::ThreadDispatchPolicy::kIPAddressHashing);

    // Game Server on port 4242
    evpp::EventLoop game_loop;
    evpp::http::Server game_server(2);
    game_server.SetThreadDispatchPolicy(evpp::ThreadDispatchPolicy::kIPAddressHashing);

    game_server.RegisterDefaultHandler([dispatcher](evpp::EventLoop* loop,
        const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
            dispatcher->handle(loop, ctx, cb);
        });

 /*   if (!dlc_server.Init({ static_cast<uint16_t>(dlc_port) })) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER, "Failed to initialize DLC HTTP server.");
        return;
    }*/

    if (!game_server.Init({ static_cast<uint16_t>(game_port) })) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER, "Failed to initialize Game HTTP server.");
        return;
    }


    // Start servers
    //dlc_server.Start();
    game_server.Start();

    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Server IP: %s", server_ip.c_str());
    //logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "DLC HTTP Server started on port %d", dlc_port);
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Game HTTP Server started on port %d", game_port);

    // Run event loops
    //std::thread dlc_thread([&dlc_loop]() {
    //    dlc_loop.Run();
    //    });

    game_loop.Run();
    //dlc_thread.join();
}