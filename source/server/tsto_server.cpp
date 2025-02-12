#include <std_include.hpp>

#include "tsto_server.hpp"
#include <chrono>
#include <ctime>
#include <iomanip>

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include "compression.hpp"  

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

            auto now = std::chrono::system_clock::now();
            auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();

            std::string response =
                "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                "<Time><epochMilliseconds>" +
                std::to_string(millis) +
                "</epochMilliseconds></Time>";

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_LOBBY, "[LOBBY TIME] Sending time: %lld ms", millis);

            cb(response);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LOBBY, "[LOBBY TIME] Error: %s", ex.what()); ctx->set_response_http_code(500); cb("");
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

	//TODO : Implement this properly
    void TSTOServer::handle_proto_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();
            size_t land_start = uri.find("/protocurrency/") + 14;
            size_t land_end = uri.find("/", land_start);
            std::string land_id = uri.substr(land_start, land_end - land_start);

            Data::CurrencyData currency;
            currency.set_id(land_id);                     
            currency.set_vctotalpurchased(0);
            currency.set_vctotalawarded(0);
            currency.set_vcbalance(1234567);             

            int64_t current_time = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count();

            currency.set_createdat(current_time);
            currency.set_updatedat(current_time);

            headers::set_protobuf_response(ctx);
            cb(utils::serialization::serialize_protobuf(currency));

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
                "[CURRENCY] Sent currency data for land_id: %s", land_id.c_str());
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
                "[PLUGIN EVENT PROTOLAND] Request from %s for land_id: %s",
                ctx->remote_ip().data(), land_id.c_str());

            Data::EventsMessage event;

            std::string serialized;
            if (!event.SerializeToString(&serialized)) {
                throw std::runtime_error("Failed to serialize EventsMessage");
            }

            headers::set_protobuf_response(ctx);
            ctx->set_response_http_code(200); // 200

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_LAND,
                "[PLUGIN EVENT PROTOLAND] Sending event data for land_id: %s (size: %zu bytes)",
                land_id.c_str(), serialized.length());

            cb(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PLUGIN EVENT PROTOLAND] Error: %s", ex.what());
            ctx->set_response_http_code(500); 
            cb(""); 
        }
    }

}