#include <std_include.hpp>
#include "auth.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "tsto_server.hpp"
#include <serialization.hpp>
#include "tsto/includes/session.hpp" 
#include <AuthData.pb.h> 
#include <sstream>
#include <random>
#include "tsto/database/database.hpp"
#include <vector>
#include <cctype> 
#include <algorithm> 

namespace tsto::auth {

    //URL decoder
    std::string url_decode(const std::string& encoded) {
        std::string result;
        result.reserve(encoded.length());
        
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                int value = 0;
                std::istringstream is(encoded.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                } else {
                    result += encoded[i];
                }
            } else if (encoded[i] == '+') {
                result += ' ';
            } else {
                result += encoded[i];
            }
        }
        
        return result;
    }

    //gen random string
    std::string generate_random_string(size_t length) {
        static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::string result;
        result.resize(length);

        for (size_t i = 0; i < length; ++i) {
            result[i] = charset[rand() % (sizeof(charset) - 1)];
        }

        return result;
    }

    //gen random token
    std::string Auth::generate_random_token(const std::string& user_id) {
        std::string random_string = generate_random_string(32);
        return random_string;
    }
    
    std::string Auth::generate_access_token(const std::string& user_id) {
        // Format: AT0:2.0:3.0:86400:[random_string]:[user_id]:[random_suffix]
        std::string random_part = generate_random_string(32);
        std::string random_suffix = generate_random_string(5);
        
        return "AT0:2.0:3.0:86400:" + random_part + ":" + user_id + ":" + random_suffix;
    }

    std::string Auth::generate_random_code() {
        return generate_random_string(40); // 40 char random string
    }

    //gen access token with type prefix
    std::string Auth::generate_typed_access_token(const std::string& type, const std::string& user_id) {
        std::stringstream ss;
        ss << type << "0:2.0:3.0:86400:";
        ss << generate_random_string(32); // 32 char random string
        ss << ":" << user_id << ":";
        ss << generate_random_string(5);  // 5 char random suffix
        return ss.str();
    }

    std::string Auth::generate_access_code(const std::string& user_id) {
        return Auth::generate_typed_access_token("AC", user_id);
    }

    void Auth::handle_check_token(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            helpers::log_request(ctx);

            //user_id and token type from the URL
            std::string uri = std::string(ctx->uri());
            std::vector<std::string> parts;
            size_t pos = 0;
            while ((pos = uri.find('/')) != std::string::npos) {
                parts.push_back(uri.substr(0, pos));
                uri.erase(0, pos + 1);
            }
            parts.push_back(uri);
            
            // URL format: /mh/games/bg_gameserver_plugin/checkToken/{user_id}/protoWholeLandToken/
            std::string user_id;
            if (parts.size() >= 6) {
                user_id = parts[5]; // Extract user_id from URL path
            }
            
            //token from the request headers
            std::string token;
            const char* auth_header = ctx->FindRequestHeader("mh_auth_params");
            if (auth_header && auth_header[0] != '\0') {
                token = auth_header;
            } else {
                //token from nucleus_token header
                auth_header = ctx->FindRequestHeader("nucleus_token");
                if (auth_header && auth_header[0] != '\0') {
                    token = auth_header;
                }
            }
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH, 
                "[CHECK TOKEN] User ID: %s, Token: %s", user_id.c_str(), token.c_str());
            
            auto& db = tsto::database::Database::get_instance();
            std::string email;
            bool valid = false;
            
            if (!token.empty()) {
                valid = db.validate_access_token(token, email);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CHECK TOKEN] Token validation result: %s, email: %s", 
                    valid ? "valid" : "invalid", email.c_str());
            }
            
            auto& session = tsto::Session::get();

            Data::TokenData response;
            response.set_sessionkey(session.session_key);
            response.set_expirationdate(0);

            std::string serialized;
            if (!response.SerializeToString(&serialized)) {
                throw std::runtime_error("Failed to serialize TokenData");
            }

            headers::set_protobuf_response(ctx);

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
                "[CONNECT AUTH] Request from %s", std::string(ctx->remote_ip()).c_str());

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Request params:\n"
                "URI: %s\n"
                "Query string: %s\n"
                "authenticator_login_type: %s\n"
                "client_id: %s\n"
                "apiVer: %s\n"
                "serverEnvironment: %s\n"
                "redirect_uri: %s\n"
                "release_type: %s\n"
                "response_type: %s",
                std::string(ctx->uri()).c_str(),
                std::string(ctx->original_uri()).c_str(),
                std::string(ctx->GetQuery("authenticator_login_type")).c_str(),
                std::string(ctx->GetQuery("client_id")).c_str(),
                std::string(ctx->GetQuery("apiVer")).c_str(),
                std::string(ctx->GetQuery("serverEnvironment")).c_str(),
                std::string(ctx->GetQuery("redirect_uri")).c_str(),
                std::string(ctx->GetQuery("release_type")).c_str(),
                std::string(ctx->GetQuery("response_type")).c_str()
            );

            auto client_id = std::string(ctx->GetQuery("client_id"));
            bool is_ios = (client_id == "simpsons4-ios-client");

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Client platform: %s", is_ios ? "iOS" : "Android");

            auto auth_type = std::string(ctx->GetQuery("authenticator_login_type"));
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Auth type: %s", auth_type.c_str());

            if (auth_type == "mobile_ea_account") {
                auto sig = std::string(ctx->GetQuery("sig"));
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Signature present: %s", sig.empty() ? "No" : "Yes");

                if (!sig.empty()) {
                    auto sig_parts = utils::string::split(sig, ".");
                    if (sig_parts.empty() || sig_parts[0].empty()) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[CONNECT AUTH] Invalid signature format");
                        ctx->set_response_http_code(403);
                        cb("");
                        return;
                    }

                    try {
                        std::string decoded = utils::cryptography::base64::decode(
                            sig_parts[0] + std::string(3 - (sig_parts[0].length() % 3), '=')
                        );
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                            "[CONNECT AUTH] Decoded signature: %s", decoded.c_str());
                    }
                    catch (const std::exception& ex) {
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                            "[CONNECT AUTH] Failed to decode signature: %s", sig.c_str());
                    }
                }

                rapidjson::Document doc;
                doc.SetObject();
                auto& allocator = doc.GetAllocator();

                //gen a random code and token
                std::string random_code = Auth::generate_random_code();
                std::string token = Auth::generate_random_token("47082");
                std::string lnglv_token = utils::cryptography::base64::encode(token);
                
                doc.AddMember("code", rapidjson::Value(random_code.c_str(), allocator), allocator);
                doc.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Generated response:\n"
                    "code: %s\n"
                    "lnglv_token: %s",
                    random_code.c_str(),
                    lnglv_token.c_str());

                headers::set_json_response(ctx);
                std::string response = utils::serialization::serialize_json(doc);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Sending response: %s", response.c_str());
                cb(response);
            }
            else {
                rapidjson::Document doc;
                doc.SetObject();
                auto& allocator = doc.GetAllocator();

                //gen a random code for anonymous auth
                std::string random_code = Auth::generate_random_code();
                
                auto& session = tsto::Session::get();
                auto& db = tsto::database::Database::get_instance();
                
                //gen a temporary email for anonymous user
                std::string anon_email = "anonymous_" + generate_random_string(10) + "@temp.com";
                
                // Store the anonymous user in the database with the access code
                // This will allow the code to be validated later
                std::string access_token = Auth::generate_typed_access_token("AT", session.user_user_id);
                
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Storing anonymous user: email=%s, user_id=%s, access_token=%s, access_code=%s",
                    anon_email.c_str(), session.user_user_id.c_str(), access_token.c_str(), random_code.c_str());
                
                //store the user data in the database
                int64_t mayhem_id = db.get_next_mayhem_id();
                if (!db.store_user_id(anon_email, session.user_user_id, access_token, mayhem_id, random_code)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Failed to store anonymous user in database");
                }
                
                doc.AddMember("code", rapidjson::Value(random_code.c_str(), allocator), allocator);

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Generated anonymous response:\n"
                    "code: %s", random_code.c_str());

                headers::set_json_response(ctx);
                std::string response = utils::serialization::serialize_json(doc);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Sending anonymous response: %s", response.c_str());
                cb(response);
            }

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Sent response for client_id: %s",
                client_id.c_str());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Auth::handle_connect_tokeninfo(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Request from %s", std::string(ctx->remote_ip()).c_str());

            auto& db = tsto::database::Database::get_instance();
            auto& session = tsto::Session::get();

            session.reinitialize(); // hacky fix to restart if delete token not called

            std::string access_token;
            
            // Extract access token from query params
            auto query_token = ctx->GetQuery("access_token");
            
            if (!query_token.empty()) {
                //token from URL query parameter
                access_token = query_token;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKENINFO] Found access token in URL query: %s", 
                    access_token.substr(0, 10).c_str()); // Log only first 10 chars for security
            } else {
                //token from Authorization header
                const char* auth_header = ctx->FindRequestHeader("access_token");
                if (auth_header) {
                    std::string auth_str(auth_header);
                    // Trim whitespace from the beginning and end
                    auth_str.erase(0, auth_str.find_first_not_of(" \t\r\n"));
                    auth_str.erase(auth_str.find_last_not_of(" \t\r\n") + 1);
                    
                    // Bearer token
                    if (auth_str.find("Bearer ") == 0) {
                        access_token = auth_str.substr(7); // Skip "Bearer " prefix
                        //trim any whitespace after the Bearer prefix
                        access_token.erase(0, access_token.find_first_not_of(" \t\r\n"));
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                            "[CONNECT TOKENINFO] Found access token in Authorization header: %s", 
                            access_token.substr(0, 10).c_str()); // Log only first 10 chars for security
                    } else {
                        // If it's not prefixed with "Bearer ", use the whole header value
                        access_token = auth_str;
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                            "[CONNECT TOKENINFO] Found non-Bearer access token in Authorization header: %s", 
                            access_token.substr(0, 10).c_str()); // Log only first 10 chars for security
                    }
                } else {
                    std::string body = ctx->body().ToString();
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT TOKENINFO] Request body: %s", body.c_str());
                    
                    if (!body.empty() && body[0] == '{') {
                        try {
                            rapidjson::Document doc;
                            doc.Parse(body.c_str());
                            
                            if (!doc.HasParseError() && doc.IsObject()) {
                                // Try to find access_token in the JSON
                                if (doc.HasMember("access_token") && doc["access_token"].IsString()) {
                                    access_token = doc["access_token"].GetString();
                                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                                        "[CONNECT TOKENINFO] Found access token in request body JSON: %s", 
                                        access_token.substr(0, 10).c_str()); // Log only first 10 chars for security
                                }
                            }
                        } catch (const std::exception& ex) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[CONNECT TOKENINFO] Error parsing JSON body: %s", ex.what());
                        }
                    }
                    
                    if (access_token.empty() && !body.empty()) {
                        const char* content_type = ctx->FindRequestHeader("Content-Type");
                        if (content_type && std::string(content_type).find("application/x-www-form-urlencoded") != std::string::npos) {
                            size_t pos = body.find("access_token=");
                            if (pos != std::string::npos) {
                                pos += 13; // Length of "access_token="
                                size_t end_pos = body.find('&', pos);
                                if (end_pos != std::string::npos) {
                                    access_token = body.substr(pos, end_pos - pos);
                                } else {
                                    access_token = body.substr(pos);
                                }
                                // URL decode the token if needed
                                access_token = url_decode(access_token);
                                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                                    "[CONNECT TOKENINFO] Found access token in form data: %s", 
                                    access_token.substr(0, 10).c_str()); // Log only first 10 chars for security
                            }
                        }
                    }
                }
            }
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Final Access Token: %s...", 
                access_token.empty() ? "empty" : (access_token.substr(0, 10) + "...").c_str());

            //Print some common request headers
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH, "[CONNECT TOKENINFO] Request Headers:");
            const char* auth_header = ctx->FindRequestHeader("Authorization");
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH, 
                "  Authorization: %s", auth_header ? auth_header : "null");
            const char* content_type = ctx->FindRequestHeader("Content-Type");
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH, 
                "  Content-Type: %s", content_type ? content_type : "null");
            const char* user_agent = ctx->FindRequestHeader("User-Agent");
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH, 
                "  User-Agent: %s", user_agent ? user_agent : "null");

            //check if we have a valid access token
            if (access_token.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKENINFO] No access token provided in request");
                ctx->set_response_http_code(400);
                cb("{\"error\":\"invalid_request\",\"error_description\":\"Missing access token\"}");
                return;
            }

            headers::set_json_response(ctx);
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            //scan token exists in database
            std::string email;
            bool found = false;
            std::string user_id;
            int64_t mayhem_id = 0;
            
            found = db.validate_access_token(access_token, email);
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Database lookup result: found=%s, email=%s", 
                found ? "true" : "false", found ? email.c_str() : "none");
            
            if (found) {
                if (db.get_user_id(email, user_id)) {
                    session.user_user_id = user_id;
                    session.access_token = access_token;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT TOKENINFO] User logged in successfully: email=%s, user_id=%s",
                        email.c_str(), user_id.c_str());
                } else {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[CONNECT TOKENINFO] Failed to get user_id for email: %s", email.c_str());
                    ctx->set_response_http_code(401);
                    cb("{\"error\":\"invalid_token\",\"error_description\":\"Invalid token\"}");
                    return;
                }
            } else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKENINFO] Invalid access token: %s", access_token.c_str());
                ctx->set_response_http_code(401);
                cb("{\"error\":\"invalid_token\",\"error_description\":\"Invalid token\"}");
                return;
            }

            doc.AddMember("client_id", "simpsons4-android-client", allocator);
            doc.AddMember("scope", "offline basic.antelope.links.bulk openid signin antelope-rtm-readwrite search.identity basic.antelope basic.identity basic.persona antelope-inbox-readwrite", allocator);
            doc.AddMember("expires_in", 4242, allocator);
            doc.AddMember("pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            doc.AddMember("pid_type", "AUTHENTICATOR_ANONYMOUS", allocator);
            doc.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            doc.AddMember("persona_id", rapidjson::Value(user_id.c_str(), allocator), allocator);

            //check if we need to include additional fields based on headers
            bool include_underage = false;
            bool include_authenticators = false;
            bool include_stopprocess = false;
            bool include_tid = false;

            auto check_underage = ctx->FindRequestHeader("x-check-underage");
            if (check_underage && std::string(check_underage) == "true") {
                include_underage = true;
            }

            auto include_auth = ctx->FindRequestHeader("x-include-authenticators");
            if (include_auth && std::string(include_auth) == "true") {
                include_authenticators = true;
            }

            auto include_stop = ctx->FindRequestHeader("x-include-stopprocess");
            if (include_stop && std::string(include_stop) == "true") {
                include_stopprocess = true;
            }

            auto include_telemetry = ctx->FindRequestHeader("x-include-tid");
            if (include_telemetry && std::string(include_telemetry) == "true") {
                include_tid = true;
            }

            if (include_underage) {
                doc.AddMember("is_underage", rapidjson::Value(), allocator); //null value
            }

            if (include_authenticators) {
                rapidjson::Value authenticators(rapidjson::kArrayType);
                rapidjson::Value authenticator(rapidjson::kObjectType);
                authenticator.AddMember("authenticator_type", "AUTHENTICATOR_ANONYMOUS", allocator);
                authenticator.AddMember("authenticator_pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                authenticators.PushBack(authenticator, allocator);
                doc.AddMember("authenticators", authenticators, allocator);
            }

            if (include_stopprocess) {
                doc.AddMember("stopProcess", "OFF", allocator);
            }

            if (include_tid) {
                doc.AddMember("telemetry_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            }

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Sending response for client_id: %s", ctx->GetQuery("client_id").c_str());

            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH, 
                "Error in handle_connect_tokeninfo: %s", ex.what()); 
            ctx->set_response_http_code(500); 
            cb("");
        }
    }

    void Auth::handle_connect_token(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Request from %s", std::string(ctx->remote_ip()).c_str());

            auto client_id = std::string(ctx->GetQuery("client_id"));
            bool is_ios = (client_id == "simpsons4-ios-client");

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Determined platform: %s", is_ios ? "iOS" : "Android");

            // Get the code from the query parameters
            std::string code = std::string(ctx->GetQuery("code"));
            if (code.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] Missing code parameter");
                ctx->set_response_http_code(400);
                cb("{\"error\":\"invalid_request\",\"error_description\":\"Missing code parameter\"}");
                return;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Received code: %s", code.c_str());

            // Get database instance
            auto& db = tsto::database::Database::get_instance();
            
            // Look up the email by access code
            std::string email;
            if (!db.get_email_by_access_code(code, email)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] Invalid access code: %s", code.c_str());
                ctx->set_response_http_code(400);
                cb("{\"error\":\"invalid_grant\",\"error_description\":\"Invalid code\"}");
                return;
            }

            // Get the user ID
            std::string user_id;
            if (!db.get_user_id(email, user_id)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] Failed to get user ID for email: %s", email.c_str());
                ctx->set_response_http_code(500);
                cb("{\"error\":\"server_error\",\"error_description\":\"Failed to get user data\"}");
                return;
            }

            // Get the access token
            std::string access_token;
            if (!db.get_access_token(email, access_token)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] Failed to get access token for email: %s", email.c_str());
                ctx->set_response_http_code(500);
                cb("{\"error\":\"server_error\",\"error_description\":\"Failed to get user data\"}");
                return;
            }

            headers::set_json_response(ctx);
            auto& session = tsto::Session::get();
            session.user_user_id = user_id;
            session.access_token = access_token;

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            // Generate random refresh token with same format but RT0 prefix
            std::string refresh_token_base = "RT0" + access_token.substr(3);  // Replace AT0 with RT0
            std::string refresh_token = refresh_token_base + ".MpDW6wVO8Ek79nu6jxMdSQwOqP";

            std::string id_token_header = utils::cryptography::base64::encode("{\"typ\":\"JWT\",\"alg\":\"HS256\"}");

            rapidjson::Document id_token_body_doc;
            id_token_body_doc.SetObject();
            id_token_body_doc.AddMember("aud", rapidjson::Value(client_id.c_str(), allocator), allocator);
            id_token_body_doc.AddMember("iss", "accounts.ea.com", allocator);
            id_token_body_doc.AddMember("iat", (int64_t)(std::time(nullptr)), allocator);
            id_token_body_doc.AddMember("exp", (int64_t)(std::time(nullptr)) + 4242, allocator);
            id_token_body_doc.AddMember("pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            id_token_body_doc.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            id_token_body_doc.AddMember("persona_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            id_token_body_doc.AddMember("pid_type", "AUTHENTICATOR_ANONYMOUS", allocator);
            id_token_body_doc.AddMember("auth_time", 0, allocator);

            std::string id_token_body = utils::cryptography::base64::encode(
                utils::serialization::serialize_json(id_token_body_doc)
            );

            std::string hex_sig = "2Tok8RykmQD41uWDv5mI7JTZ7NIhcZAIPtiBm4Z5";
            std::string id_token = id_token_header + "." + id_token_body + "." + 
                utils::cryptography::base64::encode(hex_sig);

            doc.AddMember("access_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
            doc.AddMember("token_type", "Bearer", allocator);
            doc.AddMember("expires_in", 4242, allocator);
            doc.AddMember("refresh_token", rapidjson::Value(refresh_token.c_str(), allocator), allocator);
            doc.AddMember("refresh_token_expires_in", 4242, allocator);
            doc.AddMember("id_token", rapidjson::Value(id_token.c_str(), allocator), allocator);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Sending %s response for client_id: %s",
                is_ios ? "iOS" : "Android", client_id.c_str());

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