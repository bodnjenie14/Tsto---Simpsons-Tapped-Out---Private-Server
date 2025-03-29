#include <std_include.hpp>
#include "tsto_server.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include "utilities/compression.hpp"  
#include "utilities/configuration.hpp" 
#include "tsto/events/events.hpp"
#include "tsto/land/land.hpp"
#include "tsto/auth/auth.hpp"
#include "tsto/database/database.hpp"
#include "tsto/includes/session.hpp"

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

           if (platform == "ios") {
               doc.AddMember("bundleId", "com.ea.simpsonssocial.inc2", allocator);
               logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                   "[DIRECTION] Platform is iOS, bundleId set to: com.ea.simpsonssocial.inc2");
           }
           else {
               doc.AddMember("bundleId", "com.ea.game.simpsons4_row", allocator);
               logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                   "[DIRECTION] Platform is Android, bundleId set to: com.ea.game.simpsons4_row");
           }

           std::string clientId = "simpsons4-" + platform + "-client";
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
           doc.AddMember("packageId", "com.ea.game.simpsons4_row", allocator);

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
           
           logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
               "[DIRECTION] Using server address: %s", server_address.c_str());
           
           // Format RTM host URL based on Docker configuration
           std::string rtm_host;
           std::string base_ip = server_ip_;
           size_t colon_pos = base_ip.find(':');
           if (colon_pos != std::string::npos) {
               base_ip = base_ip.substr(0, colon_pos);
           }
           
           if (docker_enabled_) {
               // In Docker mode, use the base IP with Docker port and RTM port
               rtm_host = protocol + "://" + base_ip + ":" + std::to_string(docker_port_) + ":9000";
               logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                   "[DIRECTION] Docker mode enabled, using RTM host with Docker port: %s", rtm_host.c_str());
           } else {
               // In normal mode, just use port 9000
               rtm_host = protocol + "://" + base_ip + ":9000";
               logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                   "[DIRECTION] Using standard RTM host: %s", rtm_host.c_str());
           }

           std::vector<std::pair<const char*, std::string>> initialEntries;
           if (platform == "ios") {
               initialEntries = {
                   {"antelope.rtm.host", rtm_host},
                   {"applecert.url", "https://www.apple.com/appleca/AppleIncRootCertificate.cer"},
                   {"origincasualapp.url", protocol + "://" + server_address + "/loader/mobile/ios/"},
                   {"akamai.url", "https://cdn.skum.eamobile.com/skumasset/gameasset/"}
               };
           }
           else {
               initialEntries = {
                   {"antelope.rtm.host", rtm_host},
                   {"origincasualapp.url", protocol + "://" + server_address + "/loader/mobile/android/"},
                   {"akamai.url", "https://cdn.skum.eamobile.com/skumasset/gameasset/"}
               };
           }

           for (const auto& entry : initialEntries) {
               rapidjson::Value serverEntry(rapidjson::kObjectType);
               serverEntry.AddMember("key", rapidjson::StringRef(entry.first), allocator);
               serverEntry.AddMember("value", rapidjson::StringRef(entry.second.c_str()), allocator);
               serverData.PushBack(serverEntry, allocator);
           }

           // Add Docker configuration to serverData if enabled
           if (docker_enabled_) {
               rapidjson::Value dockerConfigItem(rapidjson::kObjectType);
               dockerConfigItem.AddMember("key", "docker.enabled", allocator);
               dockerConfigItem.AddMember("value", "true", allocator);
               serverData.PushBack(dockerConfigItem, allocator);
               
               // Convert port to string properly
               std::string portStr = std::to_string(docker_port_);
               rapidjson::Value dockerPortItem(rapidjson::kObjectType);
               dockerPortItem.AddMember("key", "docker.port", allocator);
               dockerPortItem.AddMember("value", rapidjson::Value(portStr.c_str(), allocator), allocator);
               serverData.PushBack(dockerPortItem, allocator);
               
               logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                   "[DIRECTION] Added Docker configuration to serverData (port: %d)", docker_port_);
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
           if (docker_enabled_) {
               baseUrl += ":" + std::to_string(docker_port_);
           }
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
            } else {
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
                body_str = utils::compression::zlib::decompress_gzip(
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

    void TSTOServer::handle_progreg_code(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string body = ctx->body().ToString();
            if (body.empty()) {
                throw std::runtime_error("Empty request body");
            }

            rapidjson::Document doc;
            doc.Parse(body.c_str());
            if (doc.HasParseError() || !doc.IsObject()) {
                throw std::runtime_error("Invalid JSON in request body");
            }

            if (!doc.HasMember("email") || !doc["email"].IsString()) {
                throw std::runtime_error("Missing or invalid 'email' field in request");
            }
            std::string email = doc["email"].GetString();
            std::string filename = email;

            std::string access_token;
            const char* auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header && strncmp(auth_header, "Bearer ", 7) == 0) {
                access_token = auth_header + 7; // Skip "Bearer " prefix
            }

            if (access_token.empty()) {
                throw std::runtime_error("Missing or invalid Authorization header");
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[PROGREG CODE] Access token received: %s", access_token.c_str());

            auto& session = tsto::Session::get();
            session.reinitialize();

            auto& db = tsto::database::Database::get_instance();

            std::string existing_email;
            bool token_exists = db.get_email_by_token(access_token, existing_email);
            
            std::string user_id;
            int64_t mayhem_id = 0;
            std::string access_code;
            
            if (token_exists) {

                std::string stored_user_id;
                if (db.get_user_id(existing_email, stored_user_id)) {
                    user_id = stored_user_id;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Converting anonymous user to registered user. Email: %s -> %s, User ID: %s", 
                        existing_email.c_str(), email.c_str(), user_id.c_str());
                
                db.get_mayhem_id(existing_email, mayhem_id);
                if (mayhem_id == 0) {
                    mayhem_id = db.get_next_mayhem_id();
                }
            } else {
                user_id = session.user_user_id;
                mayhem_id = db.get_next_mayhem_id();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Could not find user_id for existing token. Generated new user_id: %s", 
                    user_id.c_str());
            }
        } else {
            std::string stored_user_id;
            if (db.get_user_id(filename, stored_user_id)) {
                user_id = stored_user_id;
                db.get_mayhem_id(filename, mayhem_id);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Using existing user_id for %s: %s", 
                    filename.c_str(), stored_user_id.c_str());
            } else {
                user_id = session.user_user_id;
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Generated new user_id for %s: %s", 
                    filename.c_str(), user_id.c_str());
                
                mayhem_id = db.get_next_mayhem_id();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Generated new mayhem_id for %s: %lld", 
                    filename.c_str(), mayhem_id);
            }
        }

        if (token_exists && existing_email != email) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[PROGREG CODE] Updating user from %s to %s with the same access token", 
                existing_email.c_str(), email.c_str());
            

            std::string empty_token = ""; 
            if (!db.update_access_token(existing_email, empty_token)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Failed to clear access token for old email: %s", existing_email.c_str());
            }
        }

        access_code = tsto::auth::Auth::generate_access_code(user_id);
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
            "[PROGREG CODE] Generated access code for %s: %s", 
            filename.c_str(), access_code.c_str());

        if (!db.store_user_id(filename, user_id, access_token, mayhem_id, access_code)) {
            throw std::runtime_error("Failed to store user data in database");
        }

        tsto::land::Land land;
        land.set_email(filename);
        if (!land.instance_load_town()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[PROGREG CODE] Failed to load/create town for user: %s", filename.c_str());
            throw std::runtime_error("Failed to load/create town");
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[PROGREG CODE] Successfully loaded/created town for user: %s with filename: %s.pb", 
            filename.c_str(), land.get_filename().c_str());

        rapidjson::Document response;
        response.SetObject();
        rapidjson::Document::AllocatorType& allocator = response.GetAllocator();

        response.AddMember("code", rapidjson::Value(access_code.c_str(), allocator), allocator);
        response.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
        response.AddMember("mayhem_id", mayhem_id, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        headers::set_json_response(ctx);
        cb(buffer.GetString());
    } catch (const std::exception& e) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "[PROGREG CODE] Error: %s", e.what());
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
            std::string land_id = uri.substr(land_start, land_end - land_start);

            //grab current session to access the email-based filename
            auto& session = tsto::Session::get();
            std::string currency_path;
            std::string user_identifier;

            //check if we're using mytown.pb (non-logged in) or email-based town (logged in)
            std::string current_town = session.town_filename.empty() ? "mytown.pb" : session.town_filename;
            
            if (current_town == "mytown.pb") {
                //non logged in user - use currency.txt
                currency_path = "towns/currency.txt";
                user_identifier = "default";
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Using legacy currency file for non-logged in user");
            } else {
                //logged in user - use email-based currency file
                std::string email_base = current_town;
                size_t pb_pos = email_base.find(".pb");
                if (pb_pos != std::string::npos) {
                    email_base = email_base.substr(0, pb_pos);
                }
                size_t txt_pos = email_base.find(".txt");
                if (txt_pos != std::string::npos) {
                    email_base = email_base.substr(0, txt_pos);
                }
                currency_path = "towns/currency_" + email_base + ".txt";
                user_identifier = email_base;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Using email-based currency file for user: %s", email_base.c_str());
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
            currency_data.set_id(land_id);
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

}