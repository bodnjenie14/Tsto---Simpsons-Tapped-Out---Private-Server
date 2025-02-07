#include <std_include.hpp>
#include "user.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

namespace tsto::user {

    void User::handle_me_personas(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_USER,
                "[ME PERSONAS] Request from %s: %s", ctx->remote_ip().data(), ctx->uri().data());

            std::string uri = ctx->uri();
            auto& session = tsto::Session::get();

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            if (uri.find("/proxy/identity/pids//personas") == 0) {

                rapidjson::Value personas(rapidjson::kObjectType);
                rapidjson::Value personaArray(rapidjson::kArrayType);

                rapidjson::Value persona(rapidjson::kObjectType);
                persona.AddMember("personaId", rapidjson::Value(session.personal_id.c_str(), allocator), allocator);
                persona.AddMember("pidId", rapidjson::Value(session.me_persona_pid_id.c_str(), allocator), allocator);
                persona.AddMember("displayName", rapidjson::Value(session.display_name.c_str(), allocator), allocator);
                persona.AddMember("name", rapidjson::Value(session.persona_name.c_str(), allocator), allocator);
                persona.AddMember("namespaceName", "cem_ea_id", allocator);
                persona.AddMember("isVisible", true, allocator);
                persona.AddMember("status", "ACTIVE", allocator);
                persona.AddMember("statusReasonCode", "", allocator);
                persona.AddMember("showPersona", "FRIENDS", allocator);
                persona.AddMember("dateCreated", "2024-12-25T0:00Z", allocator);
                persona.AddMember("lastAuthenticated", "", allocator);

                personaArray.PushBack(persona, allocator);
                personas.AddMember("persona", personaArray, allocator);
                doc.AddMember("personas", personas, allocator);
            }
            else {
                rapidjson::Value persona(rapidjson::kObjectType);
                persona.AddMember("personaId", rapidjson::Value(session.personal_id.c_str(), allocator), allocator);
                persona.AddMember("pidId", rapidjson::Value(session.me_persona_pid_id.c_str(), allocator), allocator);
                persona.AddMember("displayName", rapidjson::Value(session.me_persona_display_name.c_str(), allocator), allocator);
                persona.AddMember("name", rapidjson::Value(session.me_persona_name.c_str(), allocator), allocator);
                persona.AddMember("namespaceName", "gsp-redcrow-simpsons4", allocator);
                persona.AddMember("isVisible", true, allocator);
                persona.AddMember("status", "ACTIVE", allocator);
                persona.AddMember("statusReasonCode", "", allocator);
                persona.AddMember("showPersona", "EVERYONE", allocator);
                persona.AddMember("dateCreated", "2012-02-29T0:00Z", allocator);
                persona.AddMember("lastAuthenticated", "2024-12-28T5:25Z", allocator);
                persona.AddMember("anonymousId", rapidjson::Value(session.me_persona_anonymous_id.c_str(), allocator), allocator);

                doc.AddMember("persona", persona, allocator);
            }

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_USER,
                "[ME PERSONAS] Sending response for user_id: %s",
                uri.substr(uri.find_last_of('/') + 1).c_str());

            headers::set_json_response(ctx);

            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                "[ME PERSONAS] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void User::handle_mh_users(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_USER, "[MH USERS] Request from %s", ctx->remote_ip().data());

#ifdef DEBUG

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                "[MH USERS] Request params:\n"
                "appVer: %s\n"
                "appLang: %s\n"
                "application: %s\n"
                "applicationUserId: %s",
                ctx->GetQuery("appVer").c_str(),
                ctx->GetQuery("appLang").c_str(),
                ctx->GetQuery("application").c_str(),
                ctx->GetQuery("applicationUserId").c_str()
            );

#endif 

            auto& session = tsto::Session::get();

            Data::UsersResponseMessage response;

            response.mutable_user()->set_userid(session.user_user_id);
            response.mutable_user()->set_telemetryid(session.user_telemetry_id);
            response.mutable_token()->set_sessionkey(session.token_session_key);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_USER, "[MH USERS] Sending response for userId: %s", session.user_user_id.c_str());

            headers::set_protobuf_response(ctx);
            cb(utils::serialization::serialize_protobuf(response));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER, "[MH USERS] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void User::handle_mh_userstats(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_USER,
                "[USERSTATS] Request from %s", ctx->remote_ip().data());

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                "[USERSTATS] Request params:\n"
                "device_id: %s\n"
                "synergy_id: %s",
                ctx->GetQuery("device_id").c_str(),
                ctx->GetQuery("synergy_id").c_str()
            );

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_USER,
                "[USERSTATS] Sending 409 Conflict response");

            ctx->set_response_http_code(409);
            cb("");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                "[USERSTATS] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }



}