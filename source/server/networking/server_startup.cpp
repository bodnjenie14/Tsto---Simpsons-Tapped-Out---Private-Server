#include <std_include.hpp>
#include "server_startup.hpp"
#include "dispatcher/dispatcher.hpp"
#include "debugging/console.hpp"
#include "debugging/serverlog.hpp"
#include "configuration.hpp"
#include "../discord/discord_rpc.hpp"
#include "../updater/updater.hpp"
#include "../tsto/dashboard/dashboard.hpp"
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

    // Show server version on startup
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "=== TSTO Server %s ===", updater::get_server_version().c_str());

    const char* CONFIG_SECTION = "ServerConfig";
	
	// check config for auto updates
	bool enable_auto_update = utils::configuration::ReadBoolean(CONFIG_SECTION, "EnableAutoUpdate", true);
    if (enable_auto_update) {
        // check for updates
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Checking for updates...");
        bool updateAvailable = updater::check_for_updates();
        if (updateAvailable) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Update available - starting update process");
            updater::download_and_update();
            return; // Server will restart with new version
        }
    }

    // check config for discord integration
    bool enable_discord = utils::configuration::ReadBoolean(CONFIG_SECTION, "EnableDiscord", true);
    if (enable_discord) {
        //initialize shitcord RPC
        server::discord::DiscordRPC::Initialize("1342631539657146378");
    }

    //set initial donut amount in config if not exists
    std::string initial_donuts = utils::configuration::ReadString("Server", "InitialDonutAmount", "1000");
    utils::configuration::WriteString("Server", "InitialDonutAmount", initial_donuts);

    std::string detected_ip = get_local_ipv4();
    bool auto_detect_ip = utils::configuration::ReadBoolean(CONFIG_SECTION, "AutoDetectIP", true);
    utils::configuration::WriteBoolean(CONFIG_SECTION, "AutoDetectIP", auto_detect_ip);

    std::string server_ip;
    if (auto_detect_ip) {
        server_ip = detected_ip;
        utils::configuration::WriteString(CONFIG_SECTION, "ServerIP", server_ip);
    }
    else {
        server_ip = utils::configuration::ReadString(CONFIG_SECTION, "ServerIP", detected_ip);
    }

    int game_port = static_cast<int>(utils::configuration::ReadUnsignedInteger(CONFIG_SECTION, "GamePort", 80));
    //int dlc_port = static_cast<int>(utils::configuration::ReadUnsignedInteger(CONFIG_SECTION, "DLCPort", 3074));

    auto tsto_server = std::make_shared<tsto::TSTOServer>();
    tsto_server->server_ip_ = server_ip;
    tsto_server->server_port_ = static_cast<uint16_t>(game_port);

    //dashboard server info
    tsto::dashboard::Dashboard::set_server_info(server_ip, static_cast<uint16_t>(game_port));

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
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Game HTTP Server started on port %d", game_port);

    //discord presence if enabled
    if (enable_discord) {
        std::string details = "The Simpsons™ Tapped Out";
        std::string state = "Private Server By BodNJenie";
        server::discord::DiscordRPC::UpdatePresence(details, state);
        std::thread discord_thread([]() {
            while (true) {
                server::discord::DiscordRPC::RunCallbacks();
                std::this_thread::sleep_for(std::chrono::milliseconds(16));
            }
            });
        discord_thread.detach();
    }

    game_loop.Run();
    //dlc_thread.join();

    // clean shitcord
    if (enable_discord) {
        server::discord::DiscordRPC::Shutdown();
    }
}