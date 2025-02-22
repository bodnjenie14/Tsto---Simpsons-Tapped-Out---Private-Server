#pragma once
#include <string>
#include <memory>
#include "../../deps/discord_sdk/cpp/discord.h"

namespace server {
namespace discord {

class DiscordRPC {
public:
    static void Initialize(const std::string& client_id);
    static void Shutdown();
    static void UpdatePresence(const std::string& details, 
                             const std::string& state,
                             const std::string& large_image_key = "server_icon",
                             const std::string& large_image_text = "TSTO Private Server");
    
    static void RunCallbacks();

private:
    static std::unique_ptr<::discord::Core> core_;
    static bool is_initialized_;
};

} 
} 