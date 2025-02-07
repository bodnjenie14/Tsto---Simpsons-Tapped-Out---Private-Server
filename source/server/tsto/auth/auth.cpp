#include <std_include.hpp>
#include "auth.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "tsto_server.hpp"
#include <serialization.hpp>
#include "tsto/includes/session.hpp"
#include <AuthData.pb.h> 


namespace tsto::auth {


    void Auth::handle_check_token(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            helpers::log_request(ctx);

            std::string uri = ctx->uri();
            size_t last_slash = uri.find_last_of('/');
            std::string token = uri.substr(last_slash + 1);

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH, "[CHECK TOKEN] Token: %s", token.c_str());

            auto& session = tsto::Session::get();

            Data::TokenData response;
            response.set_sessionkey(session.session_key);
            response.set_expirationdate(0);

            std::string serialized;
            if (!response.SerializeToString(&serialized)) {
                throw std::runtime_error("Failed to serialize TokenData");
            }

            headers::set_protobuf_response;

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Sending response with session key: %s", session.session_key.c_str());

            cb(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH, "[CHECK TOKEN] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Auth::handle_connect_auth(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Request from %s", ctx->remote_ip().data());

#ifdef DEBUG

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "Request params:\n"
                "authenticator_login_type: %s\n"
                "client_id: %s\n"
                "apiVer: %s\n"
                "serverEnvironment: %s\n"
                "redirect_uri: %s\n"
                "release_type: %s\n"
                "response_type: %s",
                ctx->GetQuery("authenticator_login_type").c_str(),
                ctx->GetQuery("client_id").c_str(),
                ctx->GetQuery("apiVer").c_str(),
                ctx->GetQuery("serverEnvironment").c_str(),
                ctx->GetQuery("redirect_uri").c_str(),
                ctx->GetQuery("release_type").c_str(),
                ctx->GetQuery("response_type").c_str()
            );

#endif 

            auto client_id = ctx->GetQuery("client_id");
            bool is_ios = (client_id == "simpsons4-ios-client");



            auto auth_type = ctx->GetQuery("authenticator_login_type");
            if (auth_type == "mobile_ea_account") {
                auto sig = ctx->GetQuery("sig");
                if (!sig.empty()) {
                    auto sig_parts = utils::string::split(sig, ".");
                    if (sig_parts.empty() || sig_parts[0].empty()) {
                        ctx->set_response_http_code(403);
                        cb("");
                        return;
                    }

                    try {
                        std::string decoded = utils::cryptography::base64::decode(
                            sig_parts[0] + std::string(3 - (sig_parts[0].length() % 3), '=')
                        );
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                            "sig_parts: %s", decoded.c_str());
                    }
                    catch (const std::exception& ex) {
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                            "sig: %s", sig.c_str());
                    }
                }

                rapidjson::Document doc;
                doc.SetObject();
                auto& allocator = doc.GetAllocator();
                if (is_ios) {
                    doc.AddMember("code", "QUOgAOq3MMhJN3hQu3WMxVKvNbDKwgSQoVX3d0YS", allocator);
                    std::string lnglv_token = utils::cryptography::base64::encode(
                        "AT0:2.0:3.0:86400:KpnQGZO2SIxD0xKZEL9pBauVdBJYRO0NYD2:47082:riqac"
                    );
                    doc.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);

                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Generated iOS auth response");
                }
                else {
                    doc.AddMember("code", "QUOgAP3pKNGNI71KH72KdKjMkkFE5sO1nWWi34qGAQ", allocator);
                    std::string lnglv_token = utils::cryptography::base64::encode(
                        "AT0:2.0:3.0:86400:BUr1cFqJ6P1cFhcrIbuyYAQuJ1X0kp4RwEw:47082:rhvos"
                    );
                    doc.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);

                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Generated Android auth response");
                }

                headers::set_json_response(ctx);
                cb(utils::serialization::serialize_json(doc));
            }
            else {
                rapidjson::Document doc;
                doc.SetObject();
                auto& allocator = doc.GetAllocator();

                if (is_ios) {
                    doc.AddMember("code", "QUOgAOq3MMhJN3hQu3WMxVKvNbDKwgSQoVX3d0YS", allocator);
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Generated iOS anonymous token");
                }
                else {
                    doc.AddMember("code", "QUOgAP3pKNGNI71KH72KdKjMkkFE5sO1nWWi34qGAQ", allocator);
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Generated Android anonymous token");
                }

                headers::set_json_response(ctx);
                cb(utils::serialization::serialize_json(doc));
            }

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Sent response for client_id: %s",
                ctx->GetQuery("client_id").c_str());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Auth::handle_connect_tokeninfo(evpp::EventLoop*, const evpp::http::ContextPtr & ctx,
        const evpp::http::HTTPSendResponseCallback & cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Request from %s", ctx->remote_ip().data());

            auto& session = tsto::Session::get();

            std::string access_token;
            auto auth_header = ctx->FindRequestHeader("Authorization"); // shud be ("access_token") -  code is broke wait for update - Will pass auth :D
            if (auth_header) {
                // Remove "Bearer " prefix if present
                std::string auth_str(auth_header);
                if (auth_str.substr(0, 7) == "Bearer ") {
                    access_token = auth_str.substr(7);
                }
                else {
                    access_token = auth_str;
                }
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Access Token: %s", access_token.c_str());  

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            auto client_id = ctx->GetQuery("client_id");
            bool is_ios = (client_id == "simpsons4-ios-client");

            if (access_token.empty()) {
                doc.AddMember("client_id", rapidjson::Value(is_ios ? "simpsons4-ios-client" : "simpsons4-android-client", allocator), allocator);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKENINFO] client_id set to: %s", is_ios ? "simpsons4-ios-client" : "simpsons4-android-client");
                doc.AddMember("scope", "offline basic.antelope.links.bulk openid signin antelope-rtm-readwrite search.identity basic.antelope basic.identity basic.persona antelope-inbox-readwrite", allocator);
                doc.AddMember("expires_in", 86400, allocator);
                doc.AddMember("pid_id", session.me_persona_pid_id, allocator);
                doc.AddMember("pid_type", "AUTHENTICATOR_ANONYMOUS", allocator);
                doc.AddMember("user_id", session.token_user_id, allocator);
                doc.AddMember("persona_id", session.token_persona_id, allocator);

                rapidjson::Value authenticators(rapidjson::kArrayType);
                rapidjson::Value auth(rapidjson::kObjectType);
                auth.AddMember("authenticator_type", "AUTHENTICATOR_ANONYMOUS", allocator);
                auth.AddMember("authenticator_pid_id", session.token_authenticator_pid_id, allocator);
                authenticators.PushBack(auth, allocator);
                doc.AddMember("authenticators", authenticators, allocator);

                doc.AddMember("is_underage", false, allocator);
                doc.AddMember("stopProcess", "OFF", allocator);
                doc.AddMember("telemetry_id", session.token_telemetry_id, allocator);

                headers::set_json_response(ctx);
                cb(utils::serialization::serialize_json(doc));
                return;
            }

            if (access_token.compare(session.lnglv_token) == 0) {
                doc.AddMember("client_id", "long_live_token", allocator);
                doc.AddMember("scope", "basic.identity basic.persona", allocator);
                doc.AddMember("expires_in", 5184000, allocator); // 60 days
                doc.AddMember("pid_id", "1020010147082", allocator);
                doc.AddMember("pid_type", "NUCLEUS", allocator);
                doc.AddMember("user_id", "1020010147082", allocator);
                doc.AddMember("persona_id", rapidjson::Value(), allocator); 

                headers::set_json_response(ctx);
                cb(utils::serialization::serialize_json(doc));
                return;
            }

            if (access_token.compare(session.access_token) == 0) {
                return;
            }

            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Invalid access token");
            ctx->set_response_http_code(403);
            cb("");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }


    void Auth::handle_connect_token(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Request from %s", ctx->remote_ip().data());

            auto client_id = ctx->GetQuery("client_id");
            bool is_ios = (client_id == "simpsons4-ios-client");

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Determined platform: %s", is_ios ? "iOS" : "Android");

            headers::set_json_response(ctx);
            auto& session = tsto::Session::get();

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            std::string access_token;
            if (is_ios) {
                access_token = utils::cryptography::base64::encode(
                    "AT0:2.0:3.0:720:SHTNCm5XH18l7HfL0nefD8zQvjsNxlZF3vT:59901:rhal5"
                );
            }
            else {
                access_token = utils::cryptography::base64::encode(
                    "AT0:2.0:3.0:720:KLmNCp4XH18l7HfL0nefD8zQvjsNxlZF3vT:59901:rhal5"
                );
            }
            session.access_token = access_token;

            std::string refresh_token_body;
            if (is_ios) {
                refresh_token_body = utils::cryptography::base64::encode(
                    "RT0:2.0:3.0:1439:9WrwBPXBO03aH9mhN83m8NHkrpH5gyVcQ:59901:rhal5"
                );
            }
            else {
                refresh_token_body = utils::cryptography::base64::encode(
                    "RT0:2.0:3.0:1439:8XrwBPXBO03aH9mhN83m8NHkrpH5gyVcQ:59901:rhal5"
                );
            }
            std::string refresh_token = refresh_token_body + ".MpDW6wVO8Ek79nu6jxMdSQwOqP";

            std::string id_token_header = utils::cryptography::base64::encode("{\"typ\":\"JWT\",\"alg\":\"HS256\"}");

            rapidjson::Document id_token_body_doc;
            id_token_body_doc.SetObject();
            id_token_body_doc.AddMember("aud", rapidjson::Value(client_id.c_str(), allocator), allocator);
            id_token_body_doc.AddMember("iss", "accounts.ea.com", allocator);
            id_token_body_doc.AddMember("iat", (int64_t)(std::time(nullptr) * 1000), allocator);
            id_token_body_doc.AddMember("exp", (int64_t)(std::time(nullptr) * 1000) + 3600, allocator);
            id_token_body_doc.AddMember("pid_id", rapidjson::Value(session.me_persona_pid_id.c_str(), allocator), allocator);
            id_token_body_doc.AddMember("user_id", rapidjson::Value(session.token_user_id.c_str(), allocator), allocator);

            std::string persona_id = std::to_string(session.token_persona_id);
            id_token_body_doc.AddMember("persona_id", rapidjson::StringRef(persona_id.c_str()), allocator);
            id_token_body_doc.AddMember("pid_type", "AUTHENTICATOR_ANONYMOUS", allocator);
            id_token_body_doc.AddMember("auth_time", 0, allocator);

            std::string id_token_body = utils::cryptography::base64::encode(
                utils::serialization::serialize_json(id_token_body_doc)
            );

            std::string hex_sig;
            if (is_ios) {
                hex_sig = "033b68a1deed4f9724690b1b69923bb719c56395128128dac76066713b1e";
            }
            else {
                hex_sig = "033b68a1deed4f9724690b1b69923bb719c56395128128dac76066713b1f";
            }

            std::string bytes;
            bytes.reserve(hex_sig.length() / 2);
            for (size_t i = 0; i < hex_sig.length(); i += 2) {
                uint8_t byte = (uint8_t)std::stoi(hex_sig.substr(i, 2), nullptr, 16);
                bytes.push_back(byte);
            }

            std::string id_token_sig = utils::cryptography::base64::encode(bytes);
            std::string id_token = id_token_header + "." + id_token_body + "." + id_token_sig;

            doc.AddMember("access_token", rapidjson::StringRef(access_token.c_str()), allocator);
            doc.AddMember("token_type", "Bearer", allocator);
            doc.AddMember("expires_in", 86400, allocator);
            doc.AddMember("refresh_token", rapidjson::StringRef(refresh_token.c_str()), allocator);
            doc.AddMember("refresh_token_expires_in", 86400, allocator);
            doc.AddMember("id_token", rapidjson::StringRef(id_token.c_str()), allocator);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Sending %s response for client_id: %s",
                is_ios ? "iOS" : "Android",
                client_id.c_str());

            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "Error in handle_connect_token: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

}