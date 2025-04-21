#include <std_include.hpp>
#include "tsto_server.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include "compression.hpp"  
#include "configuration.hpp" 
#include "tsto/events/events.hpp"
#include "tsto/land/new_land.hpp"
#include "tsto/auth/auth_new.hpp"
#include "tsto/database/database.hpp"
#include "tsto/includes/session.hpp"
#include "tsto/statistics/statistics.hpp"

namespace tsto {

    std::string TSTOServer::generate_random_id() {
        return utils::cryptography::random::get_challenge();
    }

    void TSTOServer::handle_get_direction(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb, const std::string& platform) {
        headers::set_json_response(ctx);

        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_GAME,
                "[DIRECTION] Request from %s: %s (Platform: %s)",
                ctx->remote_ip().data(), ctx->uri().data(), platform.c_str());

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            doc.AddMember("DMGId", 0, allocator);
            doc.AddMember("appUpgrade", 0, allocator);

            std::string protocol = "http";
            const char* forwarded_proto = ctx->FindRequestHeader("X-Forwarded-Proto");
            if (forwarded_proto && std::string(forwarded_proto) == "https") {
                protocol = "https";
            }

            // Check for iOS platforms (both "ios" and "iphone" are valid iOS identifiers)
            if (platform == "ios" || platform == "iphone") {
                doc.AddMember("bundleId", "com.ea.simpsonssocial.inc2", allocator);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[DIRECTION] Platform is iOS, bundleId set to: com.ea.simpsonssocial.inc2");
            }
            else {
                doc.AddMember("bundleId", "com.ea.game.simpsons4_row", allocator);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[DIRECTION] Platform is Android, bundleId set to: com.ea.game.simpsons4_row");
            }

            // Normalize platform name for clientId to ensure consistency
            std::string normalized_platform = (platform == "iphone") ? "ios" : platform;
            std::string clientId = "simpsons4-" + normalized_platform + "-client";
            doc.AddMember("clientId", rapidjson::StringRef(clientId.c_str()), allocator);

            doc.AddMember("clientSecret", "D0fpQvaBKmAgBRCwGPvROmBf96zHnAuZmNepQht44SgyhbCdCfFgtUTdCezpWpbRI8N6oPtb38aOVg2y", allocator);
            doc.AddMember("disabledFeatures", rapidjson::Value(rapidjson::kArrayType), allocator);
            doc.AddMember("facebookAPIKey", "43b9130333cc984c79d06aa0cad3a0c8", allocator);
            doc.AddMember("facebookAppId", "185424538221919", allocator);
            doc.AddMember("hwId", 2363, allocator);
            doc.AddMember("mayhemGameCode", "bg_gameserver_plugin", allocator);

            std::string mdmAppKey = "simpsons-4-" + platform;
            doc.AddMember("mdmAppKey", rapidjson::StringRef(mdmAppKey.c_str()), allocator);

            doc.AddMember("millennialId", "", allocator);

            // Set packageId to match bundleId for iOS
            if (platform == "ios" || platform == "iphone") {
                doc.AddMember("packageId", "com.ea.simpsonssocial.inc2", allocator);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[DIRECTION] Platform is iOS, packageId set to: com.ea.simpsonssocial.inc2");
            }
            else {
                doc.AddMember("packageId", "com.ea.game.simpsons4_row", allocator);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[DIRECTION] Platform is Android, packageId set to: com.ea.game.simpsons4_row");
            }

            rapidjson::Value pollIntervals(rapidjson::kArrayType);
            rapidjson::Value pollInterval(rapidjson::kObjectType);
            pollInterval.AddMember("key", "badgePollInterval", allocator);
            pollInterval.AddMember("value", "300", allocator);
            pollIntervals.PushBack(pollInterval, allocator);
            doc.AddMember("pollIntervals", pollIntervals, allocator);

            doc.AddMember("productId", 48302, allocator);
            doc.AddMember("resultCode", 0, allocator);
            doc.AddMember("sellId", 857120, allocator);
            doc.AddMember("serverApiVersion", "1.0.0", allocator);

            rapidjson::Value serverData(rapidjson::kArrayType);

            std::string server_address = get_server_address();

            std::vector<std::pair<const char*, std::string>> initialEntries;
            if (platform == "ios") {
                initialEntries = {
                    {"antelope.rtm.host", protocol + "://" + server_address},
                    {"applecert.url", "https://www.apple.com/appleca/AppleIncRootCertificate.cer"},
                    {"origincasualapp.url", protocol + "://" + server_address + "/loader/mobile/ios/"},
                    {"akamai.url", protocol + "://" + server_address + "/skumasset/gameasset/"}
                };
            }
            else {
                initialEntries = {
                    {"antelope.rtm.host", protocol + "://" + server_address},
                    {"origincasualapp.url", protocol + "://" + server_address + "/loader/mobile/android/"},
                    {"akamai.url", protocol + "://" + server_address + "/skumasset/gameasset/"}
                };
            }

            for (const auto& entry : initialEntries) {
                rapidjson::Value serverEntry(rapidjson::kObjectType);
                serverEntry.AddMember("key", rapidjson::StringRef(entry.first), allocator);
                serverEntry.AddMember("value", rapidjson::StringRef(entry.second.c_str()), allocator);
                serverData.PushBack(serverEntry, allocator);
            }

            const std::vector<const char*> redirectKeys = {
                "nexus.portal", "antelope.groups.url", "service.discovery.url",
                "synergy.tracking", "antelope.friends.url", "dmg.url",
                "avatars.url", "synergy.m2u", "akamai.url", "synergy.pns",
                "mayhem.url", "group.recommendations.url", "synergy.s2s",
                "friend.recommendations.url", "geoip.url", "river.pin",
                "origincasualserver.url", "ens.url", "eadp.friends.host",
                "synergy.product", "synergy.drm", "synergy.user",
                "antelope.inbox.url", "antelope.rtm.url", "friends.url",
                "aruba.url", "synergy.cipgl", "nexus.connect",
                "synergy.director", "pin.aruba.url", "nexus.proxy"
            };

            std::string baseUrl = protocol + "://" + server_address;
            for (const auto& key : redirectKeys) {
                rapidjson::Value serverEntry(rapidjson::kObjectType);
                serverEntry.AddMember("key", rapidjson::StringRef(key), allocator);
                serverEntry.AddMember("value", rapidjson::StringRef(baseUrl.c_str()), allocator);
                serverData.PushBack(serverEntry, allocator);
            }

            doc.AddMember("serverData", serverData, allocator);
            doc.AddMember("telemetryFreq", 300, allocator);

            std::string response = utils::serialization::serialize_json(doc);
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[DIRECTION] Response for %s platform: %s", platform.c_str(), response.c_str());

            cb(response);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[DIRECTION] Error handling %s platform: %s", platform.c_str(), ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }


    void TSTOServer::handle_lobby_time(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_LOBBY, "[LOBBY TIME] Request from %s", ctx->remote_ip().data());

            headers::set_xml_response(ctx);

            // Get current event time
            auto current_event = tsto::events::Events::get_current_event();

            // Calculate the time to return based on event start time and elapsed time
            time_t current_time = std::time(nullptr);
            time_t event_time = current_event.start_time;
            time_t elapsed_time = 0;

            // If we have a valid event start time, calculate elapsed time since then
            if (event_time > 0) {
                // Get the reference time when the event was set
                time_t event_reference_time = tsto::events::Events::get_event_reference_time();

                if (event_reference_time > 0) {
                    // Calculate how much real time has passed since the reference time
                    time_t real_time_elapsed = current_time - event_reference_time;

                    // Apply that elapsed time to the event time
                    event_time += real_time_elapsed;

                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LOBBY,
                        "[LOBBY TIME] Event reference time: %lld, Real time elapsed: %lld, Adjusted event time: %lld",
                        event_reference_time, real_time_elapsed, event_time);
                }
            }
            else {
                // If no event time is available, use current time
                event_time = current_time;
            }

            // Convert to milliseconds
            auto millis = event_time * 1000;

            std::string response =
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                "<Time><epochMilliseconds>" +
                std::to_string(millis) +
                "</epochMilliseconds></Time>";

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_LOBBY,
                "[LOBBY TIME] Sending time: %lld ms (Event: %s)",
                millis,
                current_event.name.c_str());

            cb(response);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LOBBY, "[LOBBY TIME] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void TSTOServer::handle_root(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        helpers::log_request(ctx);
        headers::set_json_response(ctx);

        rapidjson::Document doc;
        doc.SetObject();
        doc.AddMember("status", "operational", doc.GetAllocator());
        doc.AddMember("server", "TSTO-Server", doc.GetAllocator());

        cb(utils::serialization::serialize_json(doc));
    }




    void TSTOServer::handle_pin_events(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_PIN,
                "[PIN EVENTS] Request from %s", ctx->remote_ip().data());

            std::string body_str;
            const auto* encoding = ctx->FindRequestHeader("Content-Encoding");

            if (encoding && strcmp(encoding, "gzip") == 0) {
                const auto& body = ctx->body();
                body_str = utils::compression::zlib::decompress(
                    std::string(body.data(), body.size())
                );
            }
            else {
                const auto& body = ctx->body();
                body_str = std::string(body.data(), body.size());
            }

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            doc.AddMember("status", "ok", allocator);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_PIN,
                "[PIN EVENTS] Sending response: {\"status\": \"ok\"}");

            headers::set_json_response(ctx);
            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_PIN,
                "Error in handle_pin_events: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }


    void TSTOServer::handle_geoage_requirements(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_SERVER_HTTP, "[GEO AGE] Request from %s", ctx->remote_ip().data());

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            rapidjson::Value geoAgeReq(rapidjson::kObjectType);
            geoAgeReq.AddMember("minLegalRegAge", 13, allocator);
            geoAgeReq.AddMember("minAgeWithConsent", "3", allocator);
            geoAgeReq.AddMember("minLegalContactAge", 13, allocator);
            geoAgeReq.AddMember("country", "US", allocator);

            doc.AddMember("geoAgeRequirements", geoAgeReq, allocator);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_SERVER_HTTP, "[GEO AGE] Sending response: %s",
                utils::serialization::serialize_json(doc).c_str());

            headers::set_json_response(ctx);
            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Error in handle_geoage_requirements: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void TSTOServer::handle_plugin_event(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        helpers::log_request(ctx);
        headers::set_protobuf_response(ctx);

        Data::EventsMessage response;

        cb(utils::serialization::serialize_protobuf(response));
    }

    void TSTOServer::handle_friend_data(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            Data::GetFriendDataResponse friend_data;

            std::string serialized;
            if (!friend_data.SerializeToString(&serialized)) {
                throw std::runtime_error("Failed to serialize GetFriendDataResponse");
            }

            headers::set_protobuf_response(ctx);
            cb(serialized);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
                "[FRIEND DATA] Sent empty friend data response");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[FRIEND DATA] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void TSTOServer::handle_friend_data_origin(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_GAME,
                "[FRIEND DATA] Request from %s", ctx->remote_ip().data());

            Data::GetFriendDataResponse friend_data;

            std::string serialized;
            if (!friend_data.SerializeToString(&serialized)) {
                throw std::runtime_error("Failed to serialize GetFriendDataResponse");
            }

            headers::set_protobuf_response(ctx);
            cb(serialized);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
                "[FRIEND DATA] Sent empty friend data response");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[FRIEND DATA] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }


    void TSTOServer::handle_proto_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();
            size_t land_start = uri.find("/protocurrency/") + 14;
            size_t land_end = uri.find("/", land_start);
            std::string mayhem_id_from_url = uri.substr(land_start, land_end - land_start);

            // Get the database instance
            auto& db = tsto::database::Database::get_instance();
            std::string email;
            std::string user_id;
            std::string mayhem_id;
            std::string access_token;
            bool user_found = false;

            // First priority: Check if we can find a user by the mayhem ID from the URI
            if (!mayhem_id_from_url.empty()) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Attempting to match user by mayhem ID from URL: %s", mayhem_id_from_url.c_str());

                if (db.get_email_by_mayhem_id(mayhem_id_from_url, email)) {
                    // Found a user with this mayhem ID from URL
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Found user by mayhem ID from URL: %s -> %s",
                        mayhem_id_from_url.c_str(), email.c_str());
                    mayhem_id = mayhem_id_from_url;
                    user_found = true;
                }
            }

            // Second priority: Check if we have a mh_uid header to help identify the correct user
            if (!user_found) {
                const char* mh_uid_header = ctx->FindRequestHeader("mh_uid");
                if (mh_uid_header) {
                    std::string requested_mayhem_id = mh_uid_header;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Found mh_uid header (mayhem_id): %s", requested_mayhem_id.c_str());

                    // mh_uid header contains the mayhem_id, not user_id
                    std::string mayhem_email;
                    if (db.get_email_by_mayhem_id(requested_mayhem_id, mayhem_email)) {
                        // Found a user with this mayhem ID from mh_uid header
                        email = mayhem_email;
                        mayhem_id = requested_mayhem_id;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[CURRENCY] Found user by mayhem ID from mh_uid header: %s -> %s",
                            requested_mayhem_id.c_str(), email.c_str());
                        user_found = true;
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
                            "[CURRENCY] Could not find user with mayhem ID from mh_uid header: %s",
                            requested_mayhem_id.c_str());
                    }
                }
            }

            // Last priority: Try to find user by access token
            if (!user_found) {
                const char* auth_header = ctx->FindRequestHeader("mh_auth_params");
                if (!auth_header) {
                    auth_header = ctx->FindRequestHeader("nucleus_token");
                }

                if (auth_header) {
                    access_token = auth_header;
                    if (db.get_email_by_token(access_token, email)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[CURRENCY] Found user by access token: %s", email.c_str());
                        user_found = true;
                    }
                }
            }

            // If no user found by any method, return error
            if (!user_found) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[CURRENCY] No valid user found for proto_currency request");
                ctx->set_response_http_code(403);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"403\" type=\"BAD_REQUEST\" field=\"Invalid AccessToken, User ID, or Mayhem ID\"/>");
                return;
            }

            // Get the user_id associated with this email if we don't have it yet
            if (user_id.empty() && !db.get_user_id(email, user_id)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[CURRENCY] No user_id found for email: %s", email.c_str());
                ctx->set_response_http_code(404);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"404\" type=\"NOT_FOUND\" field=\"user_id\"/>");
                return;
            }

            // Get mayhem_id if we don't have it yet
            if (mayhem_id.empty()) {
                db.get_mayhem_id(email, mayhem_id);
            }

            // Set the correct currency path based on the authenticated user's email
            bool use_legacy_mode = utils::configuration::ReadBoolean("Land", "UseLegacyMode", false);
            bool user_is_anonymous = email.find('@') == std::string::npos;

            std::string currency_path;
            std::string user_identifier;
            std::string town_filename;

            if (user_is_anonymous) {
                if (use_legacy_mode) {
                    // Legacy mode: all anonymous users share the same currency file
                    currency_path = "towns/currency.txt";
                    user_identifier = "legacy_anonymous";
                    town_filename = "mytown.pb";
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Anonymous user detected, using legacy shared filename: %s",
                        currency_path.c_str());
                }
                else {
                    // Modern mode: each anonymous user gets a unique currency file
                    currency_path = "towns/anon_" + user_id + ".txt";
                    user_identifier = "anon_" + user_id;
                    town_filename = "anon_" + user_id + ".pb";
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Anonymous user detected, using unique filename: %s",
                        currency_path.c_str());
                }
            }
            else {
                currency_path = "towns/" + email + ".txt";
                user_identifier = email;
                town_filename = email + ".pb";
            }

            std::filesystem::create_directories("towns");

            int balance = std::stoi(utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));
            if (std::filesystem::exists(currency_path)) {
                std::ifstream input(currency_path);
                if (input.good()) {
                    input >> balance;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Loaded existing currency data for user: %s (Balance: %d)",
                        user_identifier.c_str(), balance);
                }
                input.close();
            }
            else {
                std::ofstream output(currency_path);
                output << balance;
                output.close();

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Created new currency data for user: %s with initial balance: %d",
                    user_identifier.c_str(), balance);
            }

            Data::CurrencyData currency_data;
            currency_data.set_id(mayhem_id_from_url);
            currency_data.set_vctotalpurchased(0);
            currency_data.set_vctotalawarded(balance);
            currency_data.set_vcbalance(balance);
            currency_data.set_createdat(1715911362);
            currency_data.set_updatedat(std::time(nullptr));

            std::string response;
            if (currency_data.SerializeToString(&response)) {
                headers::set_protobuf_response(ctx);
                cb(response);

                logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Sent currency data for user: %s (Balance: %d)",
                    user_identifier.c_str(), balance);
            }
            else {
                throw std::runtime_error("Failed to serialize currency data");
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[CURRENCY] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void TSTOServer::handle_plugin_event_protoland(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();
            size_t land_start = uri.find("/event/") + 7;
            size_t land_end = uri.find("/protoland/", land_start);
            std::string land_id = uri.substr(land_start, land_end - land_start);

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_LAND,
                "[PLUGIN EVENT PROTOLAND] Request from {}: {}",
                ctx->remote_ip().data(), land_id);

            Data::EventsMessage event;

            std::string serialized;
            if (!event.SerializeToString(&serialized)) {
                throw std::runtime_error("Failed to serialize EventsMessage");
            }

            headers::set_protobuf_response(ctx);
            ctx->set_response_http_code(200); // 200

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_LAND,
                "[PLUGIN EVENT PROTOLAND] Sending event data for land_id: {} (size: {} bytes)",
                land_id, serialized.length());

            cb(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PLUGIN EVENT PROTOLAND] Error: {}", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void TSTOServer::handle_links(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            const char* auth_header = ctx->FindRequestHeader("Authorization");
            std::string access_token;
            std::string email;
            std::string user_id;
            std::string mayhem_id;
            bool user_found = false;
            auto& db = tsto::database::Database::get_instance();

            // First priority: Check if we have a valid token
            if (auth_header && strncmp(auth_header, "Bearer ", 7) == 0) {
                access_token = auth_header + 7; // Skip "Bearer " prefix

                if (db.get_email_by_token(access_token, email) && db.get_user_id(email, user_id)) {
                    user_found = true;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[LINKS] Valid token for email: %s, user_id: %s", email.c_str(), user_id.c_str());
                }
            }

            // Second priority: Try to find user by mh_uid header (which contains mayhem_id)
            if (!user_found) {
                const char* mh_uid_header = ctx->FindRequestHeader("mh_uid");
                if (mh_uid_header) {
                    mayhem_id = mh_uid_header;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[LINKS] Attempting to find user by mayhem_id from mh_uid header: %s", mayhem_id.c_str());

                    if (db.get_email_by_mayhem_id(mayhem_id, email) && db.get_user_id(email, user_id)) {
                        user_found = true;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[LINKS] Found user by mayhem_id: %s, email: %s, user_id: %s",
                            mayhem_id.c_str(), email.c_str(), user_id.c_str());
                    }
                }
            }

            // Third priority: Try to find user by IP address in the database
            //if (!user_found) {
            //    std::string remote_ip = ctx->remote_ip();
            //    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
            //        "[LINKS] No valid token or mayhem_id, attempting to find user by IP: %s", remote_ip.c_str());

            //    if (db.get_user_by_ip(remote_ip, email) && db.get_user_id(email, user_id)) {
            //        user_found = true;
            //        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
            //            "[LINKS] Found user by IP: %s, email: %s, user_id: %s",
            //            remote_ip.c_str(), email.c_str(), user_id.c_str());
            //    }
            //}

            // If no user found, return error - we don't generate anything
            if (!user_found) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[LINKS] No valid user found in database");
                ctx->set_response_http_code(403);
                cb("{\"message\": \"No valid user found\", \"error\": \"UNAUTHORIZED\"}");
                return;
            }

            // Determine if this is an email user or anonymous user
            bool is_email_user = (email.find("anonymous_") != 0);

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            rapidjson::Value pidGamePersonaMappings(rapidjson::kObjectType);
            rapidjson::Value pidGamePersonaMapping(rapidjson::kArrayType);

            rapidjson::Value mapping(rapidjson::kObjectType);
            mapping.AddMember("newCreated", false, allocator);
            mapping.AddMember("personaId", rapidjson::Value(user_id.c_str(), allocator), allocator);

            std::string personaNamespace = "gsp-redcrow-simpsons4";
            const char* namespace_param = ctx->FindRequestHeader("personaNamespace");
            if (namespace_param) {
                personaNamespace = namespace_param;
            }
            mapping.AddMember("personaNamespace", rapidjson::Value(personaNamespace.c_str(), allocator), allocator);

            mapping.AddMember("pidGamePersonaMappingId", rapidjson::Value(user_id.c_str(), allocator), allocator);
            mapping.AddMember("pidId", rapidjson::Value(user_id.c_str(), allocator), allocator);
            mapping.AddMember("status", "ACTIVE", allocator);

            // Add authentication type based on user type
            if (is_email_user) {
                mapping.AddMember("authenticatorType", "NUCLEUS", allocator);
            }
            else {
                mapping.AddMember("authenticatorType", "AUTHENTICATOR_ANONYMOUS", allocator);
            }

            pidGamePersonaMapping.PushBack(mapping, allocator);
            pidGamePersonaMappings.AddMember("pidGamePersonaMapping", pidGamePersonaMapping, allocator);
            doc.AddMember("pidGamePersonaMappings", pidGamePersonaMappings, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            headers::set_json_response(ctx);
            cb(buffer.GetString());
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[LINKS] Error: %s", e.what());
            ctx->set_response_http_code(400);
            cb("{\"message\": \"" + std::string(e.what()) + "\"}");
        }
    }



}