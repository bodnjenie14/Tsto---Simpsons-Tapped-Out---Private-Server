#include <std_include.hpp>
#include "discord_rpc.hpp"
#include "../debugging/serverlog.hpp"

namespace server {
namespace discord {

std::unique_ptr<::discord::Core> DiscordRPC::core_;
bool DiscordRPC::is_initialized_ = false;

void DiscordRPC::Initialize(const std::string& client_id) {
    if (is_initialized_) {
        return;
    }

    ::discord::Core* core_ptr = nullptr;
    ::discord::Result result = ::discord::Core::Create(std::stoull(client_id), DiscordCreateFlags_Default, &core_ptr);
    
    if (result != ::discord::Result::Ok) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DISCORD, "Failed to initialize Discord RPC");
        return;
    }

    core_.reset(core_ptr);
    is_initialized_ = true;
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DISCORD, "Discord RPC initialized successfully");
}

void DiscordRPC::Shutdown() {
    if (!is_initialized_) {
        return;
    }

    core_.reset();
    is_initialized_ = false;
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DISCORD, "Discord RPC shutdown");
}

void DiscordRPC::UpdatePresence(const std::string& details, 
                               const std::string& state,
                               const std::string& large_image_key,
                               const std::string& large_image_text) {
    if (!is_initialized_ || !core_) {
        return;
    }

    ::discord::Activity activity{};
    activity.SetType(::discord::ActivityType::Playing);
    activity.SetDetails(state.c_str());  // Put "Private Server" text here to appear above time
    
    activity.GetAssets().SetLargeImage(large_image_key.c_str());
    activity.GetAssets().SetLargeText(large_image_text.c_str());
    
    core_->ActivityManager().UpdateActivity(activity, [](::discord::Result result) {
        if (result != ::discord::Result::Ok) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DISCORD, "Failed to update Discord presence");
        }
    });
}

void DiscordRPC::RunCallbacks() {
    if (is_initialized_ && core_) {
        core_->RunCallbacks();
    }
}

} // namespace discord
} // namespace server
