#include <std_include.hpp>
#include "device.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "ClientTelemetry.pb.h"

namespace tsto::device {

    void Device::handle_device_registration(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Request from %s", ctx->remote_ip().data());

            rapidjson::Document request_body;
            std::string body_content = ctx->body().ToString(); 
            if (request_body.Parse(body_content.c_str()).HasParseError()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Failed to parse JSON body");
                ctx->set_response_http_code(400);
                cb(R"({"status": "error", "message": "Invalid JSON in request body"})");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Extracted fields:");
            for (auto it = request_body.MemberBegin(); it != request_body.MemberEnd(); ++it) {
                if (it->name.IsString() && it->value.IsString()) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "%s: %s", it->name.GetString(), it->value.GetString());
                }
                else if (it->name.IsString()) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "%s: [non-string value]", it->name.GetString());
                }
            }

            auto& session = tsto::Session::get();

            rapidjson::Document response;
            response.SetObject();
            auto& allocator = response.GetAllocator();

            response.AddMember("deviceId", rapidjson::Value(session.device_id.c_str(), allocator), allocator);
            response.AddMember("resultCode", 0, allocator);
            response.AddMember("serverApiVersion", "1.0.0", allocator);

            headers::set_json_response(ctx);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Response sent to %s", ctx->remote_ip().data());

            cb(utils::serialization::serialize_json(response));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"status": "error", "message": "Internal Server Error"})");
        }
    }



    void Device::handle_get_anon_uid(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH, "[ANON UID] Request from %s", ctx->remote_ip().data());

#ifdef DEBUG

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "Request params:\n"
                "localization: %s\n"
                "appVer: %s\n"
                "deviceLanguage: %s\n"
                "appLang: %s\n"
                "apiVer: %s\n"
                "hwId: %s\n"
                "deviceLocale: %s\n"
                "eadeviceid: %s\n"
                "updatePriority: %s",
                ctx->GetQuery("localization").c_str(),
                ctx->GetQuery("appVer").c_str(),
                ctx->GetQuery("deviceLanguage").c_str(),
                ctx->GetQuery("appLang").c_str(),
                ctx->GetQuery("apiVer").c_str(),
                ctx->GetQuery("hwId").c_str(),
                ctx->GetQuery("deviceLocale").c_str(),
                ctx->GetQuery("eadeviceid").c_str(),
                ctx->GetQuery("updatePriority").c_str());

#endif

            headers::set_json_response(ctx);

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            uint64_t uid = utils::cryptography::random::get_integer() % 90000000000 + 10000000000;

            doc.AddMember("resultCode", 0, allocator);
            doc.AddMember("serverApiVersion", "1.0.0", allocator);
            doc.AddMember("uid", uid, allocator);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH, "[ANON UID] Sending uid: %llu", uid);

            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[ANON UID] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Device::handle_get_device_id(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH, "[GET DEVICE ID] Request from %s", ctx->remote_ip().data());

#ifdef DEBUG

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "Request params:\n"
                "localization: %s\n"
                "appVer: %s\n"
                "deviceLanguage: %s\n"
                "appLang: %s\n"
                "apiVer: %s\n"
                "hwId: %s\n"
                "deviceLocale: %s\n"
                "androidId: %s",
                ctx->GetQuery("localization").c_str(),
                ctx->GetQuery("appVer").c_str(),
                ctx->GetQuery("deviceLanguage").c_str(),
                ctx->GetQuery("appLang").c_str(),
                ctx->GetQuery("apiVer").c_str(),
                ctx->GetQuery("hwId").c_str(),
                ctx->GetQuery("deviceLocale").c_str(),
                ctx->GetQuery("androidId").c_str());

#endif 

            headers::set_json_response(ctx);

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            auto& session = tsto::Session::get();

            doc.AddMember("deviceId", rapidjson::Value(session.device_id.c_str(), allocator), allocator);
            doc.AddMember("resultCode", 0, allocator);
            doc.AddMember("serverApiVersion", "1.0.0", allocator);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH, "[GET DEVICE ID] Sending deviceId: %s", session.device_id.c_str());

            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "Error in handle_get_device_id: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }
   

    void Device::handle_validate_device_id(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH, "[VALIDATE DEVICE ID] Request from %s", ctx->remote_ip().data());

#ifdef DEBUG

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "Request params:\n"
                "appVer: %s\n"
                "deviceLanguage: %s\n"
                "appLang: %s\n"
                "apiVer: %s\n"
                "serverEnvironment: %s\n"
                "hwId: %s\n"
                "deviceLocale: %s\n"
                "eadeviceid: %s\n"
                "androidId: %s",
                ctx->GetQuery("appVer").c_str(),
                ctx->GetQuery("deviceLanguage").c_str(),
                ctx->GetQuery("appLang").c_str(),
                ctx->GetQuery("apiVer").c_str(),
                ctx->GetQuery("serverEnvironment").c_str(),
                ctx->GetQuery("hwId").c_str(),
                ctx->GetQuery("deviceLocale").c_str(),
                ctx->GetQuery("eadeviceid").c_str(),
                ctx->GetQuery("androidId").c_str());

#endif 
            headers::set_json_response(ctx);

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            auto& session = tsto::Session::get();

            doc.AddMember("deviceId", rapidjson::Value(session.device_id.c_str(), allocator), allocator);
            doc.AddMember("resultCode", 0, allocator);
            doc.AddMember("serverApiVersion", "1.0.0", allocator);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH, "[VALIDATE DEVICE ID] Sending deviceId: %s", session.device_id.c_str());

            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH, "[VALIDATE DEVICE ID] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }
}