#include <std_include.hpp>
#include "dispatcher.hpp"
#include "debugging/serverlog.hpp"
#include "file_server/file_server.hpp"
#include "tsto/tracking/tracking.hpp"
#include "tsto/device/device.hpp"
#include "tsto/game/game.hpp"
#include "tsto/auth/auth.hpp"
#include "tsto/user/user.hpp"
#include "tsto/land/land.hpp"
namespace server::dispatcher::http {

    Dispatcher::Dispatcher(std::shared_ptr<tsto::TSTOServer> server)
        : tsto_server_(server)
        , file_server_(std::make_unique<file_server::FileServer>()) {
    }

    void Dispatcher::handle(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) noexcept {
        try {


            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_SERVER_HTTP, "RemoteIP: '%s', URI: '%s', FULL URI: '%s'",
                ctx->remote_ip().data(), ctx->uri().data(), ctx->original_uri());

            const std::string& uri = ctx->uri();

            if (uri == "/") {
                tsto_server_->handle_root(loop, ctx, cb);
                return;
            }

            if (uri.find("/static") == 0) {
                file_server_->handle_dlc_download(loop, ctx, cb);
                return;
            }

            //server stuff

            if (uri.find("/director/api/") == 0) {
                // Extract platform from URL
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

            if (uri == "/probe") {  
                cb("");
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


            if (uri == "/user/api/android/getDeviceID") {
                tsto::device::Device::handle_get_device_id(loop, ctx, cb);
                return;
            }

            if (uri == "/user/api/android/validateDeviceID") {
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
                tsto::land::Land::handle_extraland_update(loop, ctx, cb);
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

            //friends


            if (uri.find("/mh/games/bg_gameserver_plugin/friendData") == 0) {
                tsto_server_->handle_friend_data(loop, ctx, cb);
                return;
            }


            //currency

            if (uri.find("/mh/games/bg_gameserver_plugin/protocurrency/") == 0) {
                tsto_server_->handle_proto_currency(loop, ctx, cb);
                return;
            }


            // if no route matched, return 404 and log to console
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP, "No handler found for URI: %s", uri.c_str());

            ctx->set_response_http_code(404);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(R"({"status": "error", "message": "Route not found"})");
        }


        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Error handling request: %s", ex.what());
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(R"({"status": "error", "message": "Internal Server Error"})");
        }
        catch (...) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Unknown error in request handler");
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(R"({"status": "error", "message": "Internal Server Error"})");
        }
    }
}