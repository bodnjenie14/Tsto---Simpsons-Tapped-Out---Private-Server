#include <std_include.hpp>
#include "user_new.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "tsto/database/database.hpp"
#include <regex>
#include "tsto/auth/token_gen.hpp"
#include "serialization.hpp"


namespace tsto::user {

    std::string extract_access_token(const evpp::http::ContextPtr& ctx, const std::string& log_prefix) {
        std::string access_token;

        const char* token_header = ctx->FindRequestHeader("access_token");
        if (token_header && token_header[0] != '\0') {
            access_token = token_header;
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                "[%s] Found access token in header: %s",
                log_prefix.c_str(), access_token.substr(0, 10).c_str());
            return access_token;
        }

        auto query_token = ctx->GetQuery("access_token");
        if (!query_token.empty()) {
            access_token = query_token;
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                "[%s] Found access token in URL query: %s",
                log_prefix.c_str(), access_token.substr(0, 10).c_str());
            return access_token;
        }

        const char* auth_header = ctx->FindRequestHeader("Authorization");
        if (auth_header) {
            std::string auth_str(auth_header);
            auth_str.erase(0, auth_str.find_first_not_of(" \t\r\n"));
            auth_str.erase(auth_str.find_last_not_of(" \t\r\n") + 1);

            if (auth_str.find("Bearer ") == 0) {
                access_token = auth_str.substr(7);
                access_token.erase(0, access_token.find_first_not_of(" \t\r\n"));
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                    "[%s] Found access token in Authorization header: %s",
                    log_prefix.c_str(), access_token.substr(0, 10).c_str());
            }
            else {
                access_token = auth_str;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                    "[%s] Found non-Bearer access token in Authorization header: %s",
                    log_prefix.c_str(), access_token.substr(0, 10).c_str());
            }
            return access_token;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
            "[%s] No access token found in request", log_prefix.c_str());
        return "";
    }

    std::string extract_user_id_from_url(const evpp::http::ContextPtr& ctx, const std::string& log_prefix) {
        std::string uri = ctx->uri();
        std::string user_id;

        if (uri.find("/proxy/identity/pids/me/personas/") == 0) {
            user_id = uri.substr(uri.find_last_of('/') + 1);
            if (user_id.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                    "[%s] Empty user ID in URL: %s", log_prefix.c_str(), uri.c_str());
                return "";
            }
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                "[%s] Extracted user ID from 'me/personas' URL: %s",
                log_prefix.c_str(), user_id.c_str());
            return user_id;
        }
        else if (uri == "/proxy/identity/pids//personas") {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                "[%s] iOS-specific empty user ID pattern detected", log_prefix.c_str());

            std::string access_token = extract_access_token(ctx, log_prefix);
            bool token_valid = false;

            if (!access_token.empty()) {
                auto& db = tsto::database::Database::get_instance();
                std::string email;

                if (db.get_email_by_token(access_token, email) && db.get_user_id(email, user_id)) {
                    token_valid = true;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                        "[%s] Found user ID from token: %s", log_prefix.c_str(), user_id.c_str());
                    return user_id;
                }
            }
        }
        else if (uri.find("/proxy/identity/pids/") == 0) {
            std::regex pattern(R"(/proxy/identity/pids/([^/]*)/personas)");
            std::smatch matches;

            if (std::regex_search(uri, matches, pattern) && matches.size() > 1) {
                user_id = matches[1].str();

                if (user_id.empty()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                        "[%s] Empty user ID in URL: %s", log_prefix.c_str(), uri.c_str());
                    return "";
                }

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                    "[%s] Extracted user ID from regex: %s", log_prefix.c_str(), user_id.c_str());
                return user_id;
            }
        }

        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
            "[%s] Could not extract user ID from URL: %s", log_prefix.c_str(), uri.c_str());
        return "";
    }

    bool get_user_info_from_token(const std::string& access_token,
        std::string& email,
        std::string& user_id,
        std::string& display_name,
        const std::string& log_prefix) {
        if (access_token.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                "[%s] Empty access token", log_prefix.c_str());
            return false;
        }

        auto& db = tsto::database::Database::get_instance();

        if (!db.validate_access_token(access_token, email) || email.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                "[%s] Invalid access token: %s",
                log_prefix.c_str(), access_token.substr(0, 10).c_str());
            return false;
        }

        if (!db.get_user_id(email, user_id) || user_id.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                "[%s] Could not find user_id for email: %s",
                log_prefix.c_str(), email.c_str());
            return false;
        }

        if (!db.get_display_name(email, display_name)) {
            display_name = "Player";
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_USER,
                "[%s] Could not find display_name for email: %s, using default",
                log_prefix.c_str(), email.c_str());
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
            "[%s] Found user info - Email: %s, User ID: %s, Display Name: %s",
            log_prefix.c_str(), email.c_str(), user_id.c_str(), display_name.c_str());

        return true;
    }

    void User::handle_me_personas(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            const std::string log_prefix = "ME PERSONAS";
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_USER,
                "[%s] Request from %s: %s", log_prefix.c_str(), ctx->remote_ip().data(), ctx->uri().data());

            std::string user_id = extract_user_id_from_url(ctx, log_prefix);
            if (user_id.empty()) {
                ctx->set_response_http_code(400);
                cb("{\"message\":\"Could not determine user ID\"}");
                return;
            }

            std::string access_token = extract_access_token(ctx, log_prefix);

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            std::string email;
            std::string display_name;
            bool is_anonymous = user_id.find("anonymous_") == 0;

            if (!access_token.empty()) {
                auto& db = tsto::database::Database::get_instance();
                if (db.validate_access_token(access_token, email)) {
                    if (!db.get_display_name(email, display_name) || display_name.empty()) {
                        display_name = is_anonymous ? "Anonymous" : "Player";
                    }

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                        "[%s] Found user info from token - Email: %s, Display Name: %s",
                        log_prefix.c_str(), email.c_str(), display_name.c_str());
                }
            }

            if (display_name.empty()) {
                display_name = is_anonymous ? "Anonymous" : "Player";
            }

            int persona_id = 0;
            try {
                persona_id = std::stoi(user_id);
            }
            catch (const std::exception& ex) {
                persona_id = std::hash<std::string>{}(user_id) % 1000000000;
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_USER,
                    "[%s] Failed to convert user_id to integer: %s, using hash: %d",
                    log_prefix.c_str(), ex.what(), persona_id);
            }

            std::string uri = ctx->uri();

            if (uri.find("/proxy/identity/pids/me/personas/") == 0) {
                rapidjson::Value persona(rapidjson::kObjectType);
                persona.AddMember("anonymousId", rapidjson::Value("user", allocator), allocator);
                persona.AddMember("dateCreated", rapidjson::Value("2024-12-12T15:42Z", allocator), allocator);
                persona.AddMember("displayName", rapidjson::Value(display_name.c_str(), allocator), allocator);
                persona.AddMember("isVisible", true, allocator);
                persona.AddMember("lastAuthenticated", rapidjson::Value("", allocator), allocator);
                persona.AddMember("name", rapidjson::Value(display_name.c_str(), allocator), allocator);
                persona.AddMember("namespaceName", rapidjson::Value(is_anonymous ? "gsp-redcrow-simpsons4" : "cem_ea_id", allocator), allocator);
                persona.AddMember("personaId", persona_id, allocator);
                persona.AddMember("pidId", persona_id, allocator);
                persona.AddMember("showPersona", rapidjson::Value("EVERYONE", allocator), allocator);
                persona.AddMember("status", rapidjson::Value("ACTIVE", allocator), allocator);
                persona.AddMember("statusReasonCode", rapidjson::Value("", allocator), allocator);

                doc.AddMember("persona", persona, allocator);
            }
            else {
                rapidjson::Value personas(rapidjson::kObjectType);
                rapidjson::Value personaArray(rapidjson::kArrayType);

                rapidjson::Value persona(rapidjson::kObjectType);
                persona.AddMember("dateCreated", rapidjson::Value("2024-11-05T18:35Z", allocator), allocator);
                persona.AddMember("displayName", rapidjson::Value(display_name.c_str(), allocator), allocator);
                persona.AddMember("isVisible", true, allocator);
                persona.AddMember("lastAuthenticated", rapidjson::Value("", allocator), allocator);
                persona.AddMember("name", rapidjson::Value(display_name.c_str(), allocator), allocator);
                persona.AddMember("namespaceName", rapidjson::Value(is_anonymous ? "gsp-redcrow-simpsons4" : "cem_ea_id", allocator), allocator);
                persona.AddMember("personaId", persona_id, allocator);
                persona.AddMember("pidId", persona_id, allocator);
                persona.AddMember("showPersona", rapidjson::Value("EVERYONE", allocator), allocator);
                persona.AddMember("status", rapidjson::Value("ACTIVE", allocator), allocator);
                persona.AddMember("statusReasonCode", rapidjson::Value("", allocator), allocator);

                personaArray.PushBack(persona, allocator);

                personas.AddMember("persona", personaArray, allocator);

                doc.AddMember("personas", personas, allocator);
            }

            headers::set_json_response(ctx);
            std::string response = utils::serialization::serialize_json(doc);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_USER,
                "[%s] Sending response", log_prefix.c_str());

            cb(response);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                "[ME PERSONAS] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\":\"Internal server error\"}");
        }
    }

    void User::handle_mh_users(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            const std::string log_prefix = "MH USERS";
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_USER,
                "[%s] Request from %s: %s", log_prefix.c_str(), ctx->remote_ip().data(), ctx->uri().data());

            std::string uri = ctx->uri();
            std::string method = ctx->GetMethod();

            if (method == "GET" && (uri == "/mh/users" || uri.find("/mh/users?") == 0)) {
                std::string access_token = extract_access_token(ctx, log_prefix);

                std::string application_user_id = ctx->GetQuery("applicationUserId");
                bool has_application_user_id = !application_user_id.empty();

                if (access_token.empty() && !has_application_user_id) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                        "[%s] No access token found in request", log_prefix.c_str());

                    headers::set_xml_response(ctx);
                    ctx->set_response_http_code(200);
                    std::string response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<Resources>\n"
                        "</Resources>";

                    logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_USER,
                        "[%s] Sending empty resources list for unauthenticated request", log_prefix.c_str());

                    cb(response);
                    return;
                }

                std::string email;
                std::string user_id;
                std::string mayhem_id;
                auto& db = tsto::database::Database::get_instance();
                bool user_found = false;

                if (!access_token.empty()) {
                    if (db.validate_access_token(access_token, email)) {
                        if (db.get_user_id(email, user_id)) {
                            if (db.get_mayhem_id(email, mayhem_id)) {
                                user_found = true;
                                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                                    "[%s] Found user from access token - Email: %s, User ID: %s, Mayhem ID: %s", 
                                    log_prefix.c_str(), email.c_str(), user_id.c_str(), mayhem_id.c_str());
                            }
                        }
                    }
                    
                    if (!user_found) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                            "[%s] Invalid access token or missing user data", log_prefix.c_str());
                    }
                }

                if (!user_found && has_application_user_id) {
                    if (db.get_email_by_user_id(application_user_id, email)) {
                        user_id = application_user_id;
                        
                        if (db.get_mayhem_id(email, mayhem_id)) {
                            user_found = true;
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                                "[%s] Found user from applicationUserId: %s", 
                                log_prefix.c_str(), application_user_id.c_str());
                        }
                    }
                    
                    if (!user_found) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                            "[%s] Could not find user with applicationUserId: %s", 
                            log_prefix.c_str(), application_user_id.c_str());
                    }
                }
                
                if (!user_found) {
                    ctx->set_response_http_code(404);
                    cb("Could not find a user with that token or applicationUserId");
                    return;
                }

                headers::set_xml_response(ctx);
                ctx->set_response_http_code(200);
                std::string response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                    "<Resources>\n"
                    "  <URI>/users/" + mayhem_id + "</URI>\n"
                    "</Resources>";

                logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_USER,
                    "[%s] Sending GET response for user: %s", log_prefix.c_str(), email.c_str());

                cb(response);
                return;
            }
            else if (method == "PUT" && (uri == "/mh/users" || uri.find("/mh/users?") == 0)) {
                std::string access_token = extract_access_token(ctx, log_prefix);

                std::string application_user_id = ctx->GetQuery("applicationUserId");

                std::string email;
                std::string user_id;
                std::string display_name;
                bool user_found = false;

                if (!access_token.empty()) {
                    if (get_user_info_from_token(access_token, email, user_id, display_name, log_prefix)) {
                        user_found = true;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                            "[%s] Found user from access token: %s", log_prefix.c_str(), user_id.c_str());
                    }
                }

                if (!user_found && !application_user_id.empty()) {
                    auto& db = tsto::database::Database::get_instance();
                    std::string stored_token;

                    if (db.get_user_by_id(application_user_id, stored_token)) {
                        user_found = true;
                        user_id = application_user_id;

                        if (db.get_email_by_user_id(user_id, email)) {
                            if (!db.get_display_name(email, display_name)) {
                                display_name = "Player";
                            }
                        }

                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                            "[%s] Found user from applicationUserId: %s", log_prefix.c_str(), user_id.c_str());
                    }
                }

                if (!user_found) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                        "[%s] Could not find user from token or applicationUserId", log_prefix.c_str());
                    ctx->set_response_http_code(404);
                    headers::set_xml_response(ctx);
                    cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<error code=\"404\" type=\"NOT_FOUND\" field=\"applicationUserId\"/>");
                    return;
                }

                std::string request_body;
                if (ctx->body().size() > 0) {
                    request_body = std::string(ctx->body().data(), ctx->body().size());
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_USER,
                        "[%s] PUT request body: %s", log_prefix.c_str(), request_body.c_str());
                }

                std::string mayhem_id;
                auto& db = tsto::database::Database::get_instance();
                if (!db.get_mayhem_id(email, mayhem_id)) {
                    mayhem_id = user_id;
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_USER,
                        "[%s] Could not get mayhem_id for user: %s, using user_id as fallback",
                        log_prefix.c_str(), user_id.c_str());
                }

                std::string session_id = "";           
                std::string session_key;
                if (!db.get_session_key(email, session_key)) {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_USER,
                        "[%s] Could not get session info for user: %s",
                        log_prefix.c_str(), user_id.c_str());
                }

                Data::UsersResponseMessage response_message;

                auto* user_info = response_message.mutable_user();
                user_info->set_userid(mayhem_id);
                user_info->set_telemetryid("42");       

                auto* token_info = response_message.mutable_token();
                token_info->set_sessionkey(session_key);

                std::string serialized_response;
                if (!response_message.SerializeToString(&serialized_response)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                        "[%s] Failed to serialize protobuf response", log_prefix.c_str());
                    ctx->set_response_http_code(500);
                    headers::set_xml_response(ctx);
                    cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
                        "<error code=\"500\" type=\"INTERNAL_SERVER_ERROR\"/>");
                    return;
                }

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                    "[%s] Sending protobuf response for user: %s with mayhem_id: %s",
                    log_prefix.c_str(), user_id.c_str(), mayhem_id.c_str());

                ctx->set_response_http_code(200);
                headers::set_protobuf_response(ctx);
                cb(serialized_response);
                return;
            }
            else if (method == "GET" && (uri == "/mh/users/me" || uri.find("/mh/users/me?") == 0)) {
                std::string access_token = extract_access_token(ctx, log_prefix);

                std::string email;
                std::string user_id;
                std::string display_name;

                if (!get_user_info_from_token(access_token, email, user_id, display_name, log_prefix)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                        "[%s] Could not get user info from token", log_prefix.c_str());
                    ctx->set_response_http_code(401);
                    cb("{\"error\":\"Unauthorized\",\"error_description\":\"Invalid access token\"}");
                    return;
                }

                bool is_anonymous = email.empty() || email.find("anonymous_") == 0;

                int persona_id = 0;
                try {
                    persona_id = std::stoi(user_id);
                }
                catch (const std::exception& ex) {
                    persona_id = std::hash<std::string>{}(user_id) % 1000000000;
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_USER,
                        "[%s] Failed to convert user_id to integer: %s, using hash: %d",
                        log_prefix.c_str(), ex.what(), persona_id);
                }

                rapidjson::Document doc;
                doc.SetObject();
                auto& allocator = doc.GetAllocator();

                doc.AddMember("userId", rapidjson::Value(user_id.c_str(), allocator), allocator);
                doc.AddMember("personaId", persona_id, allocator);
                doc.AddMember("displayName", rapidjson::Value(display_name.c_str(), allocator), allocator);
                doc.AddMember("email", rapidjson::Value(email.c_str(), allocator), allocator);
                doc.AddMember("country", "US", allocator);
                doc.AddMember("language", "en", allocator);
                doc.AddMember("dateCreated", "2023-01-01T00:00:00Z", allocator);
                doc.AddMember("status", "ACTIVE", allocator);
                doc.AddMember("tosAccepted", true, allocator);
                doc.AddMember("privacyAccepted", true, allocator);
                doc.AddMember("doNotSell", false, allocator);
                doc.AddMember("isUnderAge", false, allocator);
                doc.AddMember("isSubAccount", false, allocator);
                doc.AddMember("canReceiveEmail", true, allocator);

                rapidjson::Value authMethods(rapidjson::kArrayType);

                rapidjson::Value authMethod(rapidjson::kObjectType);
                authMethod.AddMember("methodType", rapidjson::Value(is_anonymous ? "anonymous" : "email", allocator), allocator);
                authMethod.AddMember("methodStatus", "ACTIVE", allocator);
                authMethods.PushBack(authMethod, allocator);

                doc.AddMember("authenticationMethods", authMethods, allocator);

                headers::set_json_response(ctx);
                std::string response = utils::serialization::serialize_json(doc);

                logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_USER,
                    "[%s] Sending user info response for user: %s", log_prefix.c_str(), user_id.c_str());

                cb(response);
                return;
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                    "[%s] Unsupported endpoint: %s %s", log_prefix.c_str(), method.c_str(), uri.c_str());
                ctx->set_response_http_code(404);
                cb("{\"error\":\"Not Found\",\"error_description\":\"Endpoint not supported\"}");
                return;
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                "[MH USERS] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\":\"Internal server error\"}");
        }
    }

    void User::handle_mh_userstats(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            const std::string log_prefix = "USERSTATS";
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_USER,
                "[%s] Request from %s: %s", log_prefix.c_str(), ctx->remote_ip().data(), ctx->uri().data());

            std::string access_token = extract_access_token(ctx, log_prefix);

            std::string email;
            bool valid_token = false;

            if (!access_token.empty()) {
                auto& db = tsto::database::Database::get_instance();
                valid_token = db.validate_access_token(access_token, email);

                if (valid_token) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                        "[%s] Valid access token for user: %s", log_prefix.c_str(), email.c_str());

                    std::string user_id;
                    if (db.get_user_id(email, user_id)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_USER,
                            "[%s] Found user_id: %s for email: %s",
                            log_prefix.c_str(), user_id.c_str(), email.c_str());

                        ctx->set_response_http_code(200);
                        headers::set_json_response(ctx);
                        cb("{}");
                        return;
                    }
                }
                else {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_USER,
                        "[%s] Invalid access token: %s",
                        log_prefix.c_str(), access_token.empty() ? "empty" : (access_token.substr(0, 10) + "...").c_str());
                }
            }

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_USER,
                "[%s] Sending 409 Conflict response", log_prefix.c_str());

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