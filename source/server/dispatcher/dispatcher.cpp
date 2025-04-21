#include <std_include.hpp>
#include "dispatcher.hpp"
#include "debugging/serverlog.hpp"
#include "file_server/file_server.hpp"
#include "tsto/tracking/tracking.hpp"
#include "tsto/device/device.hpp"
#include "tsto/game/game.hpp"
#include "tsto/dashboard/dashboard.hpp"
#include "tsto/dashboard/dashboard_config.hpp"
#include "tsto/dashboard/pending_uploads.hpp"
#include "tsto/dashboard/user_search.hpp"
#include "tsto/dashboard/public_users.hpp"
#include "tsto/dashboard/public_user_search.hpp"
#include "tsto/dashboard/admin_town_management.hpp"

#include "tsto/auth/auth.hpp"
#include "tsto/auth/login.hpp"
#include "tsto/user/user.hpp"
#include "tsto/land/land.hpp"
#include "tsto/events/events.hpp"
#include "regex"
#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/event_loop.h>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include "tsto/statistics/statistics.hpp"
#include "tsto/friends/friends.hpp"

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

            // Check if this is a DLC file request (static files)
            bool is_dlc_request = uri.find("/static/") == 0;
            bool is_common_dlc = false;

            if (is_dlc_request) {
                // Check if it's a common DLC file (zip, bin, dat)
                std::string file_ext;
                size_t dot_pos = uri.find_last_of(".");
                if (dot_pos != std::string::npos) {
                    file_ext = uri.substr(dot_pos + 1);
                    std::transform(file_ext.begin(), file_ext.end(), file_ext.begin(),
                        [](unsigned char c) { return std::tolower(c); });
                    is_common_dlc = (file_ext == "zip" || file_ext == "bin" || file_ext == "dat");
                }
            }

            // Only log non-DLC requests or if we're in debug mode
#ifdef DEBUG
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Received request: RemoteIP: '%s', URI: '%s', FULL URI: '%s'",
                remote_ip.c_str(), uri.c_str(), original_uri.c_str());
#else
            if (!is_common_dlc) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Received request: RemoteIP: '%s', URI: '%s', FULL URI: '%s'",
                    remote_ip.c_str(), uri.c_str(), original_uri.c_str());
            }
#endif

            // Check if this is a game client request (not a web panel request)
            bool is_game_client_request =
                uri.find("/mh/") == 0 ||
                uri.find("/director/") == 0 ||
                uri.find("/user/") == 0 ||
                uri.find("/connect/") == 0 ||
                uri.find("/proxy/") == 0 ||
                uri.find("/pinEvents") == 0;

            // Update client activity for game client requests
            // But exclude the lobby_time endpoint since it already registers the client
            if (is_game_client_request && remote_ip != "127.0.0.1" && uri != "/mh/games/lobby/time") {
                tsto::statistics::Statistics::get_instance().update_client_activity(remote_ip);
            }

            // Removed query parameter stripping

            // Normalize URI by removing consecutive slashes
            std::string normalized_uri = uri;
            size_t pos = 0;
            while ((pos = normalized_uri.find("//", pos)) != std::string::npos) {
                normalized_uri.replace(pos, 2, "/");
            }

            if (normalized_uri == "/") {
                tsto_server_->handle_root(loop, ctx, cb);
                return;
            }

            // Public user login routes
            if (normalized_uri == "/public_login" || normalized_uri == "/public_login.html") {
                tsto::dashboard::handle_public_login_page(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/public_register" || normalized_uri == "/public_register.html") {
                // Serve the public registration HTML page
                file_server_->handle_webpanel_file(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/login") {
                tsto::dashboard::handle_public_login(loop, ctx, cb);
                return;
            }
            
            if (normalized_uri == "/api/public/request_code") {
                tsto::dashboard::handle_public_request_code(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/validate_token") {
                tsto::dashboard::handle_public_validate_token(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/town_info") {
                tsto::dashboard::handle_public_town_info(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/import_town") {
                tsto::dashboard::handle_public_import_town(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/export_town") {
                tsto::dashboard::handle_public_export_town(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/delete_town") {
                tsto::dashboard::handle_public_delete_town(loop, ctx, cb);
                return;
            }
            
            // Admin town management endpoints
            if (normalized_uri == "/api/admin/view_user") {
                tsto::dashboard::handle_admin_view_user(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/admin/import_town") {
                tsto::dashboard::handle_admin_import_town(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/admin/export_town") {
                tsto::dashboard::handle_admin_export_town(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/admin/delete_town") {
                tsto::dashboard::handle_admin_delete_town(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/admin/save_currency") {
                tsto::dashboard::handle_admin_save_currency(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/admin/get_currency") {
                tsto::dashboard::handle_admin_get_currency(loop, ctx, cb);
                return;
            }


            if (normalized_uri == "/api/public/verify_code") {
                tsto::dashboard::handle_public_verify_code(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/resend_code") {
                tsto::dashboard::handle_public_resend_code(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/reset_password") {
                tsto::dashboard::handle_public_reset_password(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/update_display_name") {
                tsto::dashboard::handle_public_update_display_name(loop, ctx, cb);
                return;
            }

            if (normalized_uri.find("/api/public/get_display_name") == 0) {
                tsto::dashboard::handle_public_get_display_name(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/register_public_user") {
                tsto::dashboard::handle_register_public_user(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/currency_info") {
                tsto::dashboard::handle_public_currency_info(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/public/save_currency") {
                tsto::dashboard::handle_public_save_currency(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/public_dashboard" || normalized_uri == "/public_dashboard.html") {
                // Serve the public dashboard HTML page
                file_server_->handle_webpanel_file(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/probe") {
                cb("");
                return;
            }

            // Login system routes
            if (normalized_uri == "/login") {
                tsto::dashboard::Dashboard::handle_login_page(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/login") {
                tsto::dashboard::Dashboard::handle_login(loop, ctx, cb);
                return;
            }

            // New API endpoints for authentication
            if (normalized_uri == "/api/auth/login") {
                tsto::dashboard::Dashboard::handle_api_login(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/auth/validate_session") {
                tsto::dashboard::Dashboard::handle_api_validate_session(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/auth/logout") {
                tsto::dashboard::Dashboard::handle_api_logout(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/statistics") {
                tsto::dashboard::Dashboard::handle_statistics(loop, ctx, cb);
                return;
            }


            if (normalized_uri == "/api/security/accounts") {
                tsto::dashboard::Dashboard::handle_security_operations(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/logout") {
                tsto::dashboard::Dashboard::handle_logout(loop, ctx, cb);
                return;
            }

            // Login.html and related static files
            if (normalized_uri == "/login.html" || normalized_uri.find("/images/") == 0 || normalized_uri.find("/css/") == 0 || normalized_uri.find("/js/") == 0 || normalized_uri == "/access_denied.html") {
                file_server_->handle_webpanel_file(loop, ctx, cb);
                return;
            }

            // Public town uploader page and related endpoints
            if (normalized_uri == "/public_townuploader.html" || normalized_uri == "/public_townuploader") {
                tsto::dashboard::handle_public_town_uploader(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/submit_town_upload") {
                tsto::dashboard::handle_submit_town_upload(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/get_user_pending_towns") {
                tsto::dashboard::handle_get_user_pending_towns(loop, ctx, cb);
                return;
            }

            

            //dashboard 
            if (normalized_uri == "/dashboard") {
                // Authentication is handled inside the dashboard handler
                tsto::dashboard::Dashboard::handle_dashboard(loop, ctx, cb);
                return;
            }

            // Handle dashboard.html directly as a static file when requested explicitly
            if (normalized_uri == "/dashboard.html" || normalized_uri == "/town_operations.html" || normalized_uri == "/game_config.html" || normalized_uri == "/admin_management.html" || normalized_uri == "/user_management.html" || normalized_uri == "/user_search.html") {
                // Check if user is authenticated first
                if (!tsto::dashboard::Dashboard::is_authenticated(ctx)) {
                    tsto::dashboard::Dashboard::redirect_to_login(ctx, cb);
                    return;
                }

                // For user_management.html (formerly currency_editor.html), check if user has currency editor access
                if (normalized_uri == "/user_management.html") {
                    if (!tsto::dashboard::Dashboard::has_currency_editor_access(ctx)) {
                        tsto::dashboard::Dashboard::redirect_to_access_denied(ctx, cb);
                        return;
                    }
                }

                // For dashboard.html, check if user has admin access
                if (normalized_uri == "/dashboard.html") {
                    if (!tsto::dashboard::Dashboard::is_admin(ctx)) {
                        tsto::dashboard::Dashboard::redirect_to_access_denied(ctx, cb);
                        return;
                    }
                }

                // For user_search.html, check if user has admin access
                if (normalized_uri == "/user_search.html") {
                    if (!tsto::dashboard::Dashboard::is_admin(ctx)) {
                        tsto::dashboard::Dashboard::redirect_to_access_denied(ctx, cb);
                        return;
                    }
                }

                // For game_config.html, check if user has admin or appropriate role
                if (normalized_uri == "/game_config.html") {
                    // Only ADMIN, GAME_MODERATOR, and CONTENT_MANAGER can access
                    if (!tsto::dashboard::Dashboard::is_admin(ctx)) {
                        tsto::dashboard::Dashboard::redirect_to_access_denied(ctx, cb);
                        return;
                    }
                }

                // For admin_management.html (formerly user_management.html), check if user is authenticated
                if (normalized_uri == "/admin_management.html") {
                    // Allow all authenticated users to access admin management
                    // Access to specific features will be controlled in the UI
                    if (!tsto::dashboard::Dashboard::is_authenticated(ctx)) {
                        tsto::dashboard::Dashboard::redirect_to_login(ctx, cb);
                        return;
                    }
                }

                // Serve the HTML file directly
                file_server_->handle_webpanel_file(loop, ctx, cb);
                return;
            }

            // Protected dashboard API routes - check authentication first
            if ((normalized_uri.find("/api/") == 0 && normalized_uri.find("/api/config/game") != 0) ||
                normalized_uri.find("/dashboard/") == 0 ||
                normalized_uri == "/list_users" || normalized_uri == "/edit_user_currency" ||
                normalized_uri == "/town_operations" ||
                normalized_uri == "/game_config" ||
                // uri == "/password_management" || 
                normalized_uri == "/admin_management.html" ||
                normalized_uri == "/user_management.html" ||
                normalized_uri == "/user_search.html" ||
                normalized_uri == "/upload_town_file" ||
                normalized_uri == "/export_town" ||
                // uri == "/api/change-admin-password" || uri == "/api/change-operator-password" ||
                normalized_uri == "/api/users") {

                // Check if user is authenticated
                if (!tsto::dashboard::Dashboard::is_authenticated(ctx)) {
                    tsto::dashboard::Dashboard::redirect_to_login(ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/admin/import_town") {
                    tsto::dashboard::handle_admin_import_town(loop, ctx, cb);
                    return;
                }
                
                if (normalized_uri == "/api/admin/export_town") {
                    tsto::dashboard::handle_admin_export_town(loop, ctx, cb);
                    return;
                }
                
                if (normalized_uri == "/api/admin/delete_town") {
                    tsto::dashboard::handle_admin_delete_town(loop, ctx, cb);
                    return;
                }
                
                if (normalized_uri == "/api/admin/save_currency") {
                    tsto::dashboard::handle_admin_save_currency(loop, ctx, cb);
                    return;
                }

                // Process the authenticated request
                if (normalized_uri == "/api/get-user-save") {
                    tsto::dashboard::Dashboard::handle_get_user_save(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/save-user-save") {
                    tsto::dashboard::Dashboard::handle_save_user_save(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/list_users") {
                    tsto::dashboard::Dashboard::handle_list_users(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/dashboard/data") {
                    tsto::dashboard::Dashboard::handle_dashboard_data(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/edit_user_currency") {
                    tsto::dashboard::Dashboard::handle_edit_user_currency(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/browseDirectory") {
                    tsto::dashboard::Dashboard::handle_browse_directory(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/server/restart") {
                    tsto::dashboard::Dashboard::handle_server_restart(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/server/stop") {
                    tsto::dashboard::Dashboard::handle_server_stop(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/server/uptime") {
                    tsto::dashboard::Dashboard::handle_api_server_uptime(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/forceSaveProtoland") {
                    tsto::dashboard::Dashboard::handle_force_save_protoland(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/search-users") {
                    tsto::dashboard::UserSearch::handle_search_users(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/get-user") {
                    tsto::dashboard::UserSearch::handle_get_user(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/update-user") {
                    tsto::dashboard::UserSearch::handle_update_user(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/delete-user") {
                    tsto::dashboard::UserSearch::handle_delete_user(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/get-all-users") {
                    tsto::dashboard::UserSearch::handle_get_all_users(loop, ctx, cb);
                    return;
                }

                // Public user search endpoints
                if (normalized_uri == "/api/search-public-users") {
                    tsto::dashboard::PublicUserSearch::handle_search_public_users(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/get-public-user") {
                    tsto::dashboard::PublicUserSearch::handle_get_public_user(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/update-public-user") {
                    tsto::dashboard::PublicUserSearch::handle_update_public_user(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/delete-public-user") {
                    tsto::dashboard::PublicUserSearch::handle_delete_public_user(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/get-all-public-users") {
                    tsto::dashboard::PublicUserSearch::handle_get_all_public_users(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/update_initial_donuts") {
                    tsto::dashboard::DashboardConfig::handle_update_initial_donuts(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/updateDlcDirectory") {
                    tsto::dashboard::Dashboard::handle_update_dlc_directory(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/updateServerIp") {
                    tsto::dashboard::Dashboard::handle_update_server_ip(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/updateServerPort") {
                    tsto::dashboard::Dashboard::handle_update_server_port(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/updateBackupSettings") {
                    tsto::dashboard::DashboardConfig::handle_update_backup_settings(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/updateApiSettings") {
                    tsto::dashboard::DashboardConfig::handle_update_api_settings(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/updateLandSettings") {
                    tsto::dashboard::DashboardConfig::handle_update_land_settings(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/updateSecuritySettings") {
                    tsto::dashboard::DashboardConfig::handle_update_security_settings(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/dashboard/config") {
                    tsto::dashboard::DashboardConfig::handle_get_dashboard_config(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/events/set") {
                    tsto::events::Events::handle_events_set(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/events/adjust_time") {
                    tsto::events::Events::handle_events_adjust_time(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/events/reset_time") {
                    tsto::events::Events::handle_events_reset_time(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/events/get_time") {
                    tsto::events::Events::handle_events_get_time(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/upload_town_file") {
                    tsto::dashboard::Dashboard::handle_upload_town_file(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/export_town") {
                    tsto::dashboard::Dashboard::handle_export_town(loop, ctx, cb);
                    return;
                }

                // Route to town operations page handler
                if (normalized_uri == "/town_operations") {
                    tsto::dashboard::Dashboard::handle_town_operations(loop, ctx, cb);
                    return;
                }

                // Route to game config page handler
                if (normalized_uri == "/game_config") {
                    tsto::dashboard::Dashboard::handle_game_config(loop, ctx, cb);
                    return;
                }

                // Route to admin management page (formerly user management)
                if (normalized_uri == "/admin_management.html") {
                    tsto::dashboard::Dashboard::handle_user_management(loop, ctx, cb);
                    return;
                }

                // Route to user management page (formerly currency editor)
                if (normalized_uri == "/user_management.html") {
                    // Use file server to serve the static file
                    file_server_->handle_webpanel_file(loop, ctx, cb);
                    return;
                }
                //
                // Route to password management page
                // if (uri == "/password_management") {
                //     tsto::dashboard::Dashboard::handle_password_management(loop, ctx, cb);
                //     return;
                // }
                //
                // Password management API endpoints
                // if (uri == "/api/change-admin-password") {
                //     tsto::dashboard::Dashboard::handle_change_admin_password(loop, ctx, cb);
                //     return;
                // }
                //
                // if (uri == "/api/change-operator-password") {
                //     tsto::dashboard::Dashboard::handle_change_operator_password(loop, ctx, cb);
                //     return;
                // }
                //
                // User management API endpoint
                if (normalized_uri == "/api/users") {
                    tsto::dashboard::Dashboard::handle_users_api(loop, ctx, cb);
                    return;
                }

                // Session validation API endpoint
                if (normalized_uri == "/api/validate-session") {
                    tsto::dashboard::Dashboard::handle_validate_session(loop, ctx, cb);
                    return;
                }

                // Pending towns API endpoints
                if (normalized_uri == "/api/get_pending_towns" || normalized_uri == "/api/admin/pending-towns") {
                    tsto::dashboard::handle_get_pending_towns(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/approve_pending_town") {
                    tsto::dashboard::handle_approve_pending_town(loop, ctx, cb);
                    return;
                }

                if (normalized_uri == "/api/reject_pending_town") {
                    tsto::dashboard::handle_reject_pending_town(loop, ctx, cb);
                    return;
                }

                // Dashboard static files (excluding dashboard.html which is handled above)
                if (normalized_uri.find("/dashboard/") == 0 || normalized_uri.find("/images/") == 0 ||
                    normalized_uri == "/town_operations.js" || normalized_uri.find("/js/") == 0 ||
                    normalized_uri == "/tsto-styles.css" || normalized_uri == "/css/tsto-styles.css" ||
                    normalized_uri == "/proto/client_config.js" || normalized_uri == "/proto/gameplay_config.js") {
                    // Check authentication for dashboard static files
                    if (!tsto::dashboard::Dashboard::is_authenticated(ctx)) {
                        tsto::dashboard::Dashboard::redirect_to_login(ctx, cb);
                        return;
                    }
                    file_server_->handle_webpanel_file(loop, ctx, cb);
                    return;
                }
            }

            //configuration endpoints
            if (normalized_uri == "/api/config/game") {
                const std::string method = ctx->GetMethod();
                if (method == "GET" || method == "POST") {
                    tsto::game::Game::handle_gameplay_config(loop, ctx, cb);
                }
                else {
                    ctx->set_response_http_code(405);
                    cb("");
                }
                return;
            }

            if (normalized_uri == "/proxy/identity/links") {
                tsto_server_->handle_links(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/links") {
                tsto_server_->handle_links(loop, ctx, cb);
                return;
            }

            //dlc

            if (normalized_uri.find("/static") == 0) {
                // Only log file downloads if we're in debug mode or it's not a common DLC file
                bool is_common_dlc =
                    normalized_uri.find(".zip") != std::string::npos ||
                    normalized_uri.find(".bin") != std::string::npos ||
                    normalized_uri.find(".dat") != std::string::npos;

#ifdef DEBUG

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Handling file download: %s", normalized_uri.c_str());

#endif // DEBUG

                // **Ensure correct path resolution**
                std::string file_path = "static" + normalized_uri.substr(7); // Remove "/static" prefix

                file_server_->handle_dlc_download(loop, ctx, cb);
                return;
            }

            //server stuff

            if (normalized_uri.find("/director/api/") == 0) {
                size_t platform_start = normalized_uri.find("/api/") + 5;
                size_t platform_end = normalized_uri.find("/", platform_start);
                std::string platform = normalized_uri.substr(platform_start, platform_end - platform_start);

                if (normalized_uri.find("/getDirectionByPackage") != std::string::npos ||
                    normalized_uri.find("/getDirectionByBundle") != std::string::npos) {
                    tsto_server_->handle_get_direction(loop, ctx, cb, platform);
                    return;
                }
            }

            if (normalized_uri == "/mh/games/lobby/time") {
                tsto_server_->handle_lobby_time(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/proxy/identity/geoagerequirements") {
                tsto_server_->handle_geoage_requirements(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/proxy/identity/progreg/code") {
                tsto::login::Login::handle_progreg_code(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/pinEvents") {
                tsto_server_->handle_pin_events(loop, ctx, cb);
                return;
            }

            if (normalized_uri.find("/mh/games/bg_gameserver_plugin/event/") == 0 && normalized_uri.find("/protoland/") != std::string::npos) {
                tsto_server_->handle_plugin_event_protoland(loop, ctx, cb);
                return;
            }

            if (normalized_uri.find("/mh/games/bg_gameserver_plugin/event/") == 0) {
                tsto_server_->handle_plugin_event(loop, ctx, cb);
                return;
            }

            std::regex games_devices_pattern(R"(/games/\d+/devices)");
            if (std::regex_match(normalized_uri, games_devices_pattern)) {
                tsto::device::Device::handle_device_registration(loop, ctx, cb);
                return;
            }

            //user

            if (normalized_uri == "/mh/users") {
                tsto::user::User::handle_mh_users(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/mh/userstats") {
                tsto::user::User::handle_mh_userstats(loop, ctx, cb);
                return;
            }

            if (normalized_uri.find("/proxy/identity/pids/me/personas/") == 0 ||
                (normalized_uri.find("/proxy/identity/pids/") == 0 && normalized_uri.find("/personas") != std::string::npos)) {
                tsto::user::User::handle_me_personas(loop, ctx, cb);
                return;
            }


            if (normalized_uri == "/user/api/android/getAnonUid") {
                tsto::device::Device::handle_get_anon_uid(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/user/api/iphone/getAnonUid") {
                tsto::device::Device::handle_get_anon_uid(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/user/api/android/getDeviceID") {
                tsto::device::Device::handle_get_device_id(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/user/api/iphone/getDeviceID") {
                tsto::device::Device::handle_get_device_id(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/user/api/android/validateDeviceID") {
                tsto::device::Device::handle_validate_device_id(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/user/api/iphone/validateDeviceID") {
                tsto::device::Device::handle_validate_device_id(loop, ctx, cb);
                return;
            }

            // auth

            if (normalized_uri == "/connect/auth") {
                tsto::auth::Auth::handle_connect_auth(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/connect/token") {
                tsto::auth::Auth::handle_connect_token(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/connect/tokeninfo") {
                tsto::auth::Auth::handle_connect_tokeninfo(loop, ctx, cb);
                return;
            }

            if (normalized_uri.find("/mh/games/bg_gameserver_plugin/checkToken/") == 0) {
                tsto::auth::Auth::handle_check_token(loop, ctx, cb);
                return;
            }

            // game

            if (normalized_uri == "/mh/games/bg_gameserver_plugin/protoClientConfig/") {
                tsto::game::Game::handle_proto_client_config(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/mh/gameplayconfig") {
                tsto::game::Game::handle_gameplay_config(loop, ctx, cb);
                return;
            }


            //tracking

            if (normalized_uri == "/tracking/api/core/logEvent") {
                tsto::tracking::Tracking::handle_core_log_event(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/mh/games/bg_gameserver_plugin/trackinglog/") {
                tsto::tracking::Tracking::handle_tracking_log(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/mh/games/bg_gameserver_plugin/trackingmetrics/") {
                tsto::tracking::Tracking::handle_tracking_metrics(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/mh/clienttelemetry/") {
                tsto::tracking::Tracking::handle_client_telemetry(loop, ctx, cb);
                return;
            }

            //land

            if (normalized_uri.find("/mh/games/bg_gameserver_plugin/extraLandUpdate/") == 0) {
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

            if (normalized_uri.find("/mh/games/bg_gameserver_plugin/protoland/") == 0) {
                tsto::land::Land::handle_protoland(loop, ctx, cb);
                return;
            }

            if (normalized_uri.find("/mh/games/bg_gameserver_plugin/protoWholeLandToken/") == 0) {
                tsto::land::Land::handle_proto_whole_land_token(loop, ctx, cb);
                return;
            }

            if (normalized_uri.find("/mh/games/bg_gameserver_plugin/deleteToken/") != std::string::npos &&
                normalized_uri.find("/protoWholeLandToken/") != std::string::npos) {
                tsto::land::Land::handle_delete_token(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/mh/games/bg_gameserver_plugin/townOperations/") {
                tsto::dashboard::Dashboard::handle_town_operations(loop, ctx, cb);
                return;
            }

            //friends

            if (normalized_uri.find("/mh/games/bg_gameserver_plugin/friendData") == 0) {
                tsto_server_->handle_friend_data(loop, ctx, cb);
                return;
            }

            //currency 
            //TODO ::
            if (normalized_uri.find("/mh/games/bg_gameserver_plugin/protocurrency/") == 0) {
                tsto_server_->handle_proto_currency(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/api/auth/logout") {
                tsto::dashboard::Dashboard::handle_api_logout(loop, ctx, cb);
                return;
            }

            // User management API
            if (normalized_uri == "/users/api") {
                tsto::dashboard::Dashboard::handle_users_api(loop, ctx, cb);
                return;
            }

            if (normalized_uri == "/delete_town") {
                tsto::dashboard::Dashboard::handle_delete_town(loop, ctx, cb);
                return;
            }

            // Check if this is a web panel request

            

            // if no route matched, return 404 and log to console
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "No handler found for URI: %s", normalized_uri.c_str());

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