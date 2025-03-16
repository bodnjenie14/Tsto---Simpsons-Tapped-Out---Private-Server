#include "std_include.hpp"
#include "server_startup.hpp"
#include "debugging/serverlog.hpp"
#include "utilities/configuration.hpp"
#include "utilities/string.hpp"
#include "utilities/linux_compat.hpp"
#include "debugging/console.hpp"
#include "../discord/discord_rpc.hpp"
#include "../updater/updater.hpp"
#include "../tsto/dashboard/dashboard.hpp"
#include "../tsto_server.hpp"
#include "../dispatcher/dispatcher.hpp"
#include "../file_server/file_server.hpp"
#include "../tsto/includes/session.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <netdb.h>
#include <ifaddrs.h>
#endif

namespace server {
    std::string get_local_ipv4() {
#ifdef _WIN32
        char hostname[256];
        if (gethostname(hostname, sizeof(hostname)) == SOCKET_ERROR) {
            return "127.0.0.1";
        }

        struct addrinfo hints = {}, *result = nullptr;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        if (getaddrinfo(hostname, nullptr, &hints, &result) != 0) {
            return "127.0.0.1";
        }

        char ipstr[INET_ADDRSTRLEN];
        for (struct addrinfo* addr = result; addr != nullptr; addr = addr->ai_next) {
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)addr->ai_addr;
            inet_ntop(AF_INET, &(ipv4->sin_addr), ipstr, INET_ADDRSTRLEN);
            std::string ip(ipstr);
            if (ip != "127.0.0.1") {
                freeaddrinfo(result);
                return ip;
            }
        }

        freeaddrinfo(result);
        return "127.0.0.1";
#else
        struct ifaddrs* ifAddrStruct = nullptr;
        struct ifaddrs* ifa = nullptr;
        void* tmpAddrPtr = nullptr;
        std::string ip = "127.0.0.1";

        getifaddrs(&ifAddrStruct);

        for (ifa = ifAddrStruct; ifa != nullptr; ifa = ifa->ifa_next) {
            if (!ifa->ifa_addr) continue;
            if (ifa->ifa_addr->sa_family == AF_INET) {
                tmpAddrPtr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
                std::string current_ip(addressBuffer);
                if (current_ip != "127.0.0.1") {
                    ip = current_ip;
                    break;
                }
            }
        }

        if (ifAddrStruct != nullptr) {
            freeifaddrs(ifAddrStruct);
        }

        return ip;
#endif
    }

    bool is_running_in_docker() {
        // Check for .dockerenv file which exists in Docker containers
        std::ifstream dockerenv("/.dockerenv");
        if (dockerenv.good()) {
            return true;
        }

        // Check for cgroup which might indicate Docker
        std::ifstream cgroup("/proc/1/cgroup");
        std::string line;
        if (cgroup.is_open()) {
            while (std::getline(cgroup, line)) {
                if (line.find("docker") != std::string::npos) {
                    return true;
                }
            }
        }

        return false;
    }

    std::string get_docker_gateway_ip(int port = 0) {
        // Try to get the Docker gateway IP (usually the host machine's IP from the container's perspective)
        std::string gateway_ip = "172.17.0.1"; // Default Docker gateway IP
        
        // Try to read from /proc/net/route
        std::ifstream route("/proc/net/route");
        std::string line;
        if (route.is_open()) {
            // Skip header line
            std::getline(route, line);
            
            while (std::getline(route, line)) {
                std::istringstream iss(line);
                std::string iface, dest, gateway;
                iss >> iface >> dest >> gateway;
                
                // Default route has destination 00000000
                if (dest == "00000000") {
                    // Convert hex to IP
                    unsigned int ip;
                    std::stringstream ss;
                    ss << std::hex << gateway;
                    ss >> ip;
                    
                    // Convert to dotted decimal format
                    gateway_ip = std::to_string((ip & 0xFF)) + "." +
                                std::to_string((ip & 0xFF00) >> 8) + "." +
                                std::to_string((ip & 0xFF0000) >> 16) + "." +
                                std::to_string((ip & 0xFF000000) >> 24);
                    break;
                }
            }
        }
        
        // Add port if specified and not already in the IP
        if (port > 0 && gateway_ip.find(':') == std::string::npos) {
            gateway_ip += ":" + std::to_string(port);
        }
        
        return gateway_ip;
    }

    void initialize_servers() {  
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Initializing HTTP Servers...");

        // Show server version on startup
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "=== TSTO Server %s ===", updater::get_server_version().c_str());

        // Initialize session first
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Initializing session...");
        tsto::Session::get().reinitialize();

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
                return; 
            }
        }

        // check config for discord integration
        bool enable_discord = utils::configuration::ReadBoolean(CONFIG_SECTION, "EnableDiscord", true);
#ifdef DISCORD_SDK_ENABLED
        if (enable_discord) {
            //initialize discord RPC
            server::discord::DiscordRPC::Initialize("1342631539657146378");
        }
#else
        if (enable_discord) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Discord integration disabled - SDK not available");
        }
#endif
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

        // Read Docker port configuration
        int docker_port = static_cast<int>(utils::configuration::ReadUnsignedInteger(CONFIG_SECTION, "DockerPort", 8080));
        // Ensure the DockerPort exists in config
        utils::configuration::WriteUnsignedInteger(CONFIG_SECTION, "DockerPort", static_cast<unsigned int>(docker_port));

        // Ensure the ReverseProxy section exists in the config file
        const char* REVERSE_PROXY_SECTION = "ServerConfig.ReverseProxy";
        
        // Read existing values or use defaults
        bool reverse_proxy_enabled = utils::configuration::ReadBoolean(REVERSE_PROXY_SECTION, "Enabled", false);
        bool reverse_proxy_trust_headers = utils::configuration::ReadBoolean(REVERSE_PROXY_SECTION, "TrustHeaders", true);
        std::string reverse_proxy_force_host = utils::configuration::ReadString(REVERSE_PROXY_SECTION, "ForceHost", "");
        int reverse_proxy_force_port = static_cast<int>(utils::configuration::ReadUnsignedInteger(REVERSE_PROXY_SECTION, "ForcePort", 0));
        std::string reverse_proxy_force_protocol = utils::configuration::ReadString(REVERSE_PROXY_SECTION, "ForceProtocol", "");
        
        // Write back to ensure the section exists
        utils::configuration::WriteBoolean(REVERSE_PROXY_SECTION, "Enabled", reverse_proxy_enabled);
        utils::configuration::WriteBoolean(REVERSE_PROXY_SECTION, "TrustHeaders", reverse_proxy_trust_headers);
        utils::configuration::WriteString(REVERSE_PROXY_SECTION, "ForceHost", reverse_proxy_force_host);
        utils::configuration::WriteUnsignedInteger(REVERSE_PROXY_SECTION, "ForcePort", static_cast<unsigned int>(reverse_proxy_force_port));
        utils::configuration::WriteString(REVERSE_PROXY_SECTION, "ForceProtocol", reverse_proxy_force_protocol);

        auto tsto_server = std::make_shared<tsto::TSTOServer>();
        tsto_server->server_ip_ = server_ip;
        tsto_server->server_port_ = static_cast<uint16_t>(game_port);

        // Auto-detect Docker environment and configure reverse proxy settings
        bool in_docker = is_running_in_docker();
        if (in_docker) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, 
                "Docker environment detected, configuring for container networking");
            
            // Get Docker gateway IP without port
            std::string docker_gateway = get_docker_gateway_ip();
            
            // Enable reverse proxy with Docker-specific settings
            tsto_server->reverse_proxy_enabled_ = true;
            tsto_server->reverse_proxy_trust_headers_ = true;
            tsto_server->reverse_proxy_force_host_ = docker_gateway;
            tsto_server->reverse_proxy_force_port_ = docker_port;
            tsto_server->reverse_proxy_force_protocol_ = "https";
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, 
                "Docker configuration: Using host %s:%d for client connections", 
                docker_gateway.c_str(), docker_port);
        } else {
            // Read reverse proxy configuration from config file for non-Docker environments
            tsto_server->reverse_proxy_enabled_ = reverse_proxy_enabled;
            tsto_server->reverse_proxy_trust_headers_ = reverse_proxy_trust_headers;
            tsto_server->reverse_proxy_force_host_ = reverse_proxy_force_host;
            tsto_server->reverse_proxy_force_port_ = static_cast<uint16_t>(reverse_proxy_force_port);
            tsto_server->reverse_proxy_force_protocol_ = reverse_proxy_force_protocol;

            if (tsto_server->reverse_proxy_enabled_) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, 
                    "Reverse proxy support enabled. Host: %s, Port: %d, Protocol: %s, Trust Headers: %s",
                    tsto_server->reverse_proxy_force_host_.empty() ? "auto-detect" : tsto_server->reverse_proxy_force_host_.c_str(),
                    tsto_server->reverse_proxy_force_port_,
                    tsto_server->reverse_proxy_force_protocol_.empty() ? "auto-detect" : tsto_server->reverse_proxy_force_protocol_.c_str(),
                    tsto_server->reverse_proxy_trust_headers_ ? "yes" : "no");
            }
        }

        //dashboard server info
        tsto::dashboard::Dashboard::set_server_info(server_ip, static_cast<uint16_t>(game_port));

        auto dispatcher = std::make_shared<server::dispatcher::http::Dispatcher>(tsto_server);

        evpp::EventLoop game_loop;
        evpp::http::Server game_server(2);
        game_server.SetThreadDispatchPolicy(evpp::ThreadDispatchPolicy::kIPAddressHashing);

        game_server.RegisterDefaultHandler([dispatcher](evpp::EventLoop* loop,
            const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb) {
                dispatcher->handle(loop, ctx, cb);
            });

        if (!game_server.Init({ static_cast<uint16_t>(game_port) })) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER, "Failed to initialize Game HTTP server.");
            return;
        }

        game_server.Start();

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Server IP: %s", server_ip.c_str());
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER, "Game HTTP Server started on port %d", game_port);

#ifdef DISCORD_SDK_ENABLED
        if (enable_discord) {
            std::string details = "The Simpsons Tapped Out";
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
#endif

        game_loop.Run();

#ifdef DISCORD_SDK_ENABLED
        if (enable_discord) {
            server::discord::DiscordRPC::Shutdown();
        }
#endif
    }
}