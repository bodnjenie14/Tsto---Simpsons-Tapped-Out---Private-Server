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

            std::vector<std::pair<const char*, std::string>> initialEntries;
            if (platform == "ios") {
                initialEntries = {
                    {"antelope.rtm.host", protocol + "://" + server_address + ":9000"},
                    {"applecert.url", "https://www.apple.com/appleca/AppleIncRootCertificate.cer"},
                    {"origincasualapp.url", protocol + "://" + server_address + "/loader/mobile/ios/"},
                    {"akamai.url", "https://cdn.skum.eamobile.com/skumasset/gameasset/"}
                };
            }
            else {
                initialEntries = {
                    {"antelope.rtm.host", protocol + "://" + server_address + ":9000"},
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
            time_t event_time = current_event.start_time;

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


    void TSTOServer::handle_progreg_code(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        rapidjson::Document response;
        response.SetObject();
        auto& allocator = response.GetAllocator();

        response.AddMember("status", "error", allocator);
        response.AddMember("code", 501, allocator);
        response.AddMember("message", "Program registration not implemented", allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);

        ctx->set_response_http_code(501);
        ctx->AddResponseHeader("Content-Type", "application/json");
        cb(buffer.GetString());
    }

    void TSTOServer::handle_proto_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();
            size_t land_start = uri.find("/protocurrency/") + 14;
            size_t land_end = uri.find("/", land_start);
            std::string land_id = uri.substr(land_start, land_end - land_start);

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[CURRENCY] Processing currency request for land_id: %s", land_id.c_str());

            std::string data_directory = "towns";
            std::string currency_path = data_directory + "/currency.txt";

            std::filesystem::create_directories(data_directory);

            int balance = std::stoi(utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));
            if (std::filesystem::exists(currency_path)) {
                std::ifstream input(currency_path);
                if (input.good()) {
                    input >> balance;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Loaded existing currency data for land_id: %s (Balance: %d)",
                        land_id.c_str(), balance);
                }
                input.close();
            }
            else {
                //create new donut file with initial balance
                std::ofstream output(currency_path);
                output << balance;
                output.close();

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Created new currency data for land_id: %s with initial balance: %d",
                    land_id.c_str(), balance);
            }

            //set currency data
            Data::CurrencyData currency_data;
            currency_data.set_id(land_id);
            currency_data.set_vctotalpurchased(0);
            currency_data.set_vctotalawarded(balance);
            currency_data.set_vcbalance(balance);
            currency_data.set_createdat(1715911362);
            currency_data.set_updatedat(std::time(nullptr));

            //return the current currency data
            std::string response;
            if (currency_data.SerializeToString(&response)) {
                headers::set_protobuf_response(ctx);
                cb(response);

                logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Sent currency data for land_id: %s (Balance: %d)",
                    land_id.c_str(), balance);
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

    void TSTOServer::handle_server_restart(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        ctx->AddResponseHeader("Content-Type", "application/json");

        cb("{\"status\":\"success\",\"message\":\"Server restart initiated\"}");

        loop->RunAfter(evpp::Duration(1.0), [this]() {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Server stopping for restart...");

            char exePath[MAX_PATH];
            GetModuleFileNameA(NULL, exePath, MAX_PATH);

            STARTUPINFOA si = { sizeof(STARTUPINFOA) };
            PROCESS_INFORMATION pi;

            if (CreateProcessA(
                exePath,
                NULL,
                NULL,
                NULL,
                FALSE,
                0,
                NULL,
                NULL,
                &si,
                &pi
            )) {
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                ExitProcess(0);
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Failed to restart server: {}", GetLastError());
            }
            });
    }

    void TSTOServer::handle_dashboard(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        ctx->AddResponseHeader("Content-Type", "text/html; charset=utf-8");

        try {
            std::filesystem::path templatePath = "webpanel/dashboard.html";

            if (!std::filesystem::exists(templatePath)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Dashboard template not found at {}", templatePath.string().c_str());
                cb("Error: Dashboard template file not found");
                return;
            }

            std::ifstream file(templatePath, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to open dashboard template: {}", templatePath.string().c_str());
                cb("Error: Failed to open dashboard template");
                return;
            }

            std::stringstream template_stream;
            template_stream << file.rdbuf();
            std::string html_template = template_stream.str();

            html_template = std::regex_replace(html_template, std::regex("%SERVER_IP%"), server_ip_);
            html_template = std::regex_replace(html_template, std::regex("\\{\\{ GAME_PORT \\}\\}"), std::to_string(server_port_));

            static auto start_time = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
            auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime % std::chrono::hours(1));
            auto seconds = uptime % std::chrono::minutes(1);

            std::stringstream uptime_str;
            uptime_str << hours.count() << "h " << minutes.count() << "m " << seconds.count() << "s";

            html_template = std::regex_replace(html_template, std::regex("%UPTIME%"), uptime_str.str());

            // Get active sessions count
            //html_template = std::regex_replace(html_template, std::regex("%ACTIVE_SESSIONS%"), std::to_string(active_sessions_.load()));

            auto current_event = tsto::events::Events::get_current_event();
            std::string current_event_name = current_event.is_active ? current_event.name : "No Active Event";
            html_template = std::regex_replace(html_template, std::regex("%CURRENT_EVENT%"), current_event_name);

            int initial_donuts = std::stoi(utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));
            int current_donuts = initial_donuts;
            std::string currency_path = "towns/currency.txt";
            if (std::filesystem::exists(currency_path)) {
                std::ifstream input(currency_path);
                if (input.good()) {
                    input >> current_donuts;
                }
                input.close();
            }

            html_template = std::regex_replace(html_template, std::regex("%INITIAL_DONUTS%"), std::to_string(initial_donuts));
            html_template = std::regex_replace(html_template, std::regex("%CURRENT_DONUTS%"), std::to_string(current_donuts));

            std::stringstream rows;
            for (const auto& event_pair : tsto::events::tsto_events) {
                if (event_pair.first == 0) continue;

                rows << "<option value=\"" << event_pair.first << "\"";
                if (event_pair.first == current_event.start_time) {
                    rows << " selected";
                }
                rows << ">" << event_pair.second << "</option>\n";
            }

            size_t event_pos = html_template.find("%EVENT_ROWS%");
            if (event_pos != std::string::npos) {
                html_template.replace(event_pos, 12, rows.str());
            }

            cb(html_template);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Dashboard error: {}", ex.what());
            cb("Error: Failed to generate dashboard");
        }
    }

    void TSTOServer::handle_server_stop(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        ctx->AddResponseHeader("Content-Type", "application/json");

        cb("{\"status\":\"success\",\"message\":\"Server shutdown initiated\"}");

        loop->RunAfter(evpp::Duration(1.0), []() {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Server stopping...");
            ExitProcess(0);
            });
    }

    void TSTOServer::handle_update_initial_donuts(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.HasMember("initialDonuts") || !doc["initialDonuts"].IsInt()) {
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid request body\"}");
                return;
            }

            int initial_donuts = doc["initialDonuts"].GetInt();
            utils::configuration::WriteString("Server", "InitialDonutAmount", std::to_string(initial_donuts));

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[DONUTS] Updated initial donuts amount to: %d", initial_donuts);

            headers::set_json_response(ctx);
            cb("{\"success\": true}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[DONUTS] Error updating initial donuts: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\": \"Internal server error\"}");
        }
    }

    void TSTOServer::handle_update_current_donuts(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.HasMember("currentDonuts") || !doc["currentDonuts"].IsInt()) {
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid request body\"}");
                return;
            }

            int current_donuts = doc["currentDonuts"].GetInt();
            std::string currency_path = "towns/currency.txt";

            std::ofstream output(currency_path);
            output << current_donuts;
            output.close();

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[DONUTS] Updated current donuts amount to: %d", current_donuts);

            headers::set_json_response(ctx);
            cb("{\"success\": true}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[DONUTS] Error updating current donuts: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\": \"Internal server error\"}");
        }
    }

}