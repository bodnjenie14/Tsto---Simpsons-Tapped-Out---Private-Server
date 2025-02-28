#include <std_include.hpp>
#include "dispatcher.hpp"
#include "debugging/serverlog.hpp"
#include "file_server/file_server.hpp"
#include "tsto/tracking/tracking.hpp"
#include "tsto/device/device.hpp"
#include "tsto/game/game.hpp"
#include "tsto/dashboard/dashboard.hpp"
#include "tsto/auth/auth.hpp"
#include "tsto/user/user.hpp"
#include "tsto/land/land.hpp"
#include "tsto/events/events.hpp"
#include "regex"

namespace server::dispatcher::http {

    Dispatcher::Dispatcher(std::shared_ptr<tsto::TSTOServer> server)
        : tsto_server_(server)
        , file_server_(std::make_unique<file_server::FileServer>()) {
    }

    void Dispatcher::handle(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) noexcept {
        try {
            std::string remote_ip = std::string(ctx->remote_ip());
            std::string uri = std::string(ctx->uri());
            std::string original_uri = std::string(ctx->original_uri());

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Received request: RemoteIP: '%s', URI: '%s', FULL URI: '%s'",
                remote_ip.c_str(), uri.c_str(), original_uri.c_str());

            uri = std::regex_replace(uri, std::regex("/{2,}"), "/");

            if (uri == "/") {
                tsto_server_->handle_root(loop, ctx, cb);
                return;
            }

            if (uri == "/probe") {
                cb("");
                return;
            }

            //dashboard 

            if (uri == "/dashboard") {
                tsto::dashboard::Dashboard::handle_dashboard(loop, ctx, cb);
                return;
            }

            if (uri == "/api/get-user-save") {
                tsto::dashboard::Dashboard::handle_get_user_save(loop, ctx, cb);
                return;
            }

            if (uri == "/api/save-user-save") {
                tsto::dashboard::Dashboard::handle_save_user_save(loop, ctx, cb);
                return;
            }

            if (uri == "/list_users") {
                tsto::dashboard::Dashboard::handle_list_users(loop, ctx, cb);
                return;
            }

            if (uri == "/api/edit-user-currency") {
                tsto::dashboard::Dashboard::handle_edit_user_currency(loop, ctx, cb);
                return;
            }

            if (uri == "/api/browse-directory") {
                tsto::dashboard::Dashboard::handle_browse_directory(loop, ctx, cb);
                return;
            }

            // Handle dashboard static files
            if (uri.find("/dashboard/") == 0 || uri.find("/images/") == 0) {
                file_server_->handle_webpanel_file(loop, ctx, cb);
                return;
            }

            if (uri == "/api/server/restart") {
                tsto::dashboard::Dashboard::handle_server_restart(loop, ctx, cb);
                return;
            }

            if (uri == "/api/server/stop") {
                tsto::dashboard::Dashboard::handle_server_stop(loop, ctx, cb);
                return;
            }

            if (uri == "/api/forceSaveProtoland") {
                tsto::dashboard::Dashboard::handle_force_save_protoland(loop, ctx, cb);
                return;
            }

            if (uri == "/update_initial_donuts") {
                tsto::dashboard::Dashboard::handle_update_initial_donuts(loop, ctx, cb);
                return;
            }

            if (uri == "/update_current_donuts") {
                tsto::dashboard::Dashboard::handle_update_current_donuts(loop, ctx, cb);
                return;
            }

            if (uri == "/api/updateCurrentDonuts") {
                tsto::dashboard::Dashboard::handle_update_current_donuts(loop, ctx, cb);
                return;
            }

            if (uri == "/api/updateDlcDirectory") {
                tsto::dashboard::Dashboard::handle_update_dlc_directory(loop, ctx, cb);
                return;
            }

            if (uri == "/api/updateServerIp") {
                tsto::dashboard::Dashboard::handle_update_server_ip(loop, ctx, cb);
                return;
            }

            if (uri == "/api/updateServerPort") {
                tsto::dashboard::Dashboard::handle_update_server_port(loop, ctx, cb);
                return;
            }

            if (uri == "/api/events/set") {
                tsto::events::Events::handle_events_set(loop, ctx, cb);
                return;
            }

            //dlc

            if (uri.find("/static") == 0) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Handling file download: %s", uri.c_str());

                // **Ensure correct path resolution**
                std::string normalized_uri = uri.substr(7); // Remove "/static" prefix
                std::string file_path = "static" + normalized_uri;

                file_server_->handle_dlc_download(loop, ctx, cb);
                return;
            }

            //server stuff

            if (uri.find("/director/api/") == 0) {
                size_t platform_start = uri.find("/api/") + 5;
                size_t platform_end = uri.find("/", platform_start);
                std::string platform = uri.substr(platform_start, platform_end - platform_start);

                if (uri.find("/getDirectionByPackage") != std::string::npos ||
                    uri.find("/getDirectionByBundle") != std::string::npos) {
                    tsto_server_->handle_get_direction(loop, ctx, cb, platform);
                    return;
                }
            }

            if (uri == "/mh/games/lobby/time") {
                tsto_server_->handle_lobby_time(loop, ctx, cb);
                return;
            }

            if (uri == "/proxy/identity/geoagerequirements") {
                tsto_server_->handle_geoage_requirements(loop, ctx, cb);
                return;
            }

            if (uri == "/proxy/identity/progreg/code") {
                tsto_server_->handle_progreg_code(loop, ctx, cb);
                return;
            }

            if (uri == "/pinEvents") {
                tsto_server_->handle_pin_events(loop, ctx, cb);
                return;
            }

            if (uri.find("/mh/games/bg_gameserver_plugin/event/") == 0 && uri.find("/protoland/") != std::string::npos) {
                tsto_server_->handle_plugin_event_protoland(loop, ctx, cb);
                return;
            }

            if (uri.find("/mh/games/bg_gameserver_plugin/event/") == 0) {
                tsto_server_->handle_plugin_event(loop, ctx, cb);
                return;
            }

            std::regex games_devices_pattern(R"(/games/\d+/devices)");
            if (std::regex_match(uri, games_devices_pattern)) {
                tsto::device::Device::handle_device_registration(loop, ctx, cb);
                return;
            }

            //user

            if (uri == "/mh/users") {
                tsto::user::User::handle_mh_users(loop, ctx, cb);
                return;
            }

            if (uri == "/mh/userstats") {
                tsto::user::User::handle_mh_userstats(loop, ctx, cb);
                return;
            }

            if (uri.find("/proxy/identity/pids/me/personas/") == 0 ||
                (uri.find("/proxy/identity/pids/") == 0 && uri.find("/personas") != std::string::npos)) {
                tsto::user::User::handle_me_personas(loop, ctx, cb);
                return;
            }

            // device

            if (uri == "/user/api/android/getAnonUid") {
                tsto::device::Device::handle_get_anon_uid(loop, ctx, cb);
                return;
            }

            if (uri == "/user/api/iphone/getAnonUid") {
                tsto::device::Device::handle_get_anon_uid(loop, ctx, cb);
                return;
            }

            if (uri == "/user/api/android/getDeviceID") {
                tsto::device::Device::handle_get_device_id(loop, ctx, cb);
                return;
            }

            if (uri == "/user/api/iphone/getDeviceID") {
                tsto::device::Device::handle_get_device_id(loop, ctx, cb);
                return;
            }

            if (uri == "/user/api/android/validateDeviceID") {
                tsto::device::Device::handle_validate_device_id(loop, ctx, cb);
                return;
            }

            if (uri == "/user/api/iphone/validateDeviceID") {
                tsto::device::Device::handle_validate_device_id(loop, ctx, cb);
                return;
            }

            // auth

            if (uri == "/connect/auth") {
                tsto::auth::Auth::handle_connect_auth(loop, ctx, cb);
                return;
            }

            if (uri == "/connect/token") {
                tsto::auth::Auth::handle_connect_token(loop, ctx, cb);
                return;
            }

            if (uri == "/connect/tokeninfo") {
                tsto::auth::Auth::handle_connect_tokeninfo(loop, ctx, cb);
                return;
            }

            if (uri.find("/mh/games/bg_gameserver_plugin/checkToken/") == 0) {
                tsto::auth::Auth::handle_check_token(loop, ctx, cb);
                return;
            }

            // game

            if (uri == "/mh/games/bg_gameserver_plugin/protoClientConfig/") {
                tsto::game::Game::handle_proto_client_config(loop, ctx, cb);
                return;
            }

            if (uri == "/mh/gameplayconfig") {
                tsto::game::Game::handle_gameplay_config(loop, ctx, cb);
                return;
            }

            //tracking

            if (uri == "/tracking/api/core/logEvent") {
                tsto::tracking::Tracking::handle_core_log_event(loop, ctx, cb);
                return;
            }

            if (uri == "/mh/games/bg_gameserver_plugin/trackinglog/") {
                tsto::tracking::Tracking::handle_tracking_log(loop, ctx, cb);
                return;
            }

            if (uri == "/mh/games/bg_gameserver_plugin/trackingmetrics/") {
                tsto::tracking::Tracking::handle_tracking_metrics(loop, ctx, cb);
                return;
            }

            if (uri == "/mh/clienttelemetry/") {
                tsto::tracking::Tracking::handle_client_telemetry(loop, ctx, cb);
                return;
            }

            //land

            if (uri.find("/mh/games/bg_gameserver_plugin/extraLandUpdate/") == 0) {
                // Extract land_id from URI for extraland update
                std::string uri = ctx->uri();
                size_t land_start = uri.find("/extraLandUpdate/") + 16;
                size_t land_end = uri.find("/protoland/", land_start);
                std::string land_id = uri.substr(land_start, land_end - land_start);

                // Remove any leading or trailing slashes
                while (!land_id.empty() && land_id[0] == '/') {
                    land_id = land_id.substr(1);
                }
                while (!land_id.empty() && land_id.back() == '/') {
                    land_id.pop_back();
                }
                
                tsto::land::Land::handle_extraland_update(loop, ctx, cb, land_id);
                return;
            }

            if (uri.find("/mh/games/bg_gameserver_plugin/protoland/") == 0) {
                tsto::land::Land::handle_protoland(loop, ctx, cb);
                return;
            }

            if (uri.find("/mh/games/bg_gameserver_plugin/protoWholeLandToken/") == 0) {
                tsto::land::Land::handle_proto_whole_land_token(loop, ctx, cb);
                return;
            }

            // In your dispatcher
            if (uri.find("/mh/games/bg_gameserver_plugin/deleteToken/") != std::string::npos &&
                uri.find("/protoWholeLandToken/") != std::string::npos) {
                tsto::land::Land::handle_delete_token(loop, ctx, cb);
                return;
            }

            //friends

            if (uri.find("/mh/games/bg_gameserver_plugin/friendData") == 0) {
                tsto_server_->handle_friend_data(loop, ctx, cb);
                return;
            }

            //currency 
            //TODO ::
            if (uri.find("/mh/games/bg_gameserver_plugin/protocurrency/") == 0) {
                tsto_server_->handle_proto_currency(loop, ctx, cb);
                return;
            }

            // if no route matched, return 404 and log to console
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP, "No handler found for URI: %s", uri.c_str());

            ctx->set_response_http_code(404);
            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->AddResponseHeader("Server", "Basic-HTTPS-Server/1.0");

            std::string response = R"({"status": "error", "message": "Unknown endpoint"})";
            cb(response);
        }

        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Error handling request: %s", ex.what());
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->AddResponseHeader("Server", "Basic-HTTPS-Server/1.0");

            std::string response = R"({"status": "error", "message": "Internal Server Error"})";
            cb(response);
        }
        catch (...) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Unknown error in request handler");
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->AddResponseHeader("Server", "Basic-HTTPS-Server/1.0");

            std::string response = R"({"status": "error", "message": "Internal Server Error"})";
            cb(response);
        }
    }
}