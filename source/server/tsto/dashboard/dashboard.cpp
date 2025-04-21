#include <std_include.hpp>
#include "dashboard.hpp"
#include "configuration.hpp"
#include "debugging/serverlog.hpp"
#include "LandData.pb.h"
#include <Windows.h>
#include <ShlObj.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip> 
#include <random>
#include <ctime>

#include "tsto/land/new_land.hpp"
#include "tsto/land/backup.hpp"
#include "tsto/events/events.hpp"
#include "tsto/statistics/statistics.hpp"
#include "tsto/database/database.hpp"
#include "tsto/includes/session.hpp"
#include "headers/response_headers.hpp"
#include "cryptography.hpp"
#include "tsto/auth/user_roles.hpp"
#include "tsto/includes/server_time.hpp"

namespace tsto::dashboard {

    std::string Dashboard::server_ip_;
    uint16_t Dashboard::server_port_;

    bool Dashboard::is_authenticated(const evpp::http::ContextPtr& ctx) {
        const char* cookie_header = ctx->FindRequestHeader("Cookie");
        if (!cookie_header) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Authentication failed: No cookie header found");
            return false;
        }

        std::string cookies(cookie_header);
        std::string token;

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
            "Checking authentication with cookies: %s", cookies.c_str());

        size_t session_pos = cookies.find("tsto_session=");
        if (session_pos != std::string::npos) {
            size_t start = session_pos + 13;    
            size_t end = cookies.find(';', start);

            if (end != std::string::npos) {
                token = cookies.substr(start, end - start);
            }
            else {
                token = cookies.substr(start);
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Found session token: %s", token.c_str());
        }
        else {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "No tsto_session cookie found");
            return false;
        }

        std::string username;
        bool is_valid = UserRole::ValidateSession(token, username);

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
            "Token validation result: %s", is_valid ? "valid" : "invalid");

        return is_valid;
    }

    bool Dashboard::is_admin(const evpp::http::ContextPtr& ctx) {
        const char* cookie_header = ctx->FindRequestHeader("Cookie");
        if (!cookie_header) {
            return false;
        }

        std::string cookies(cookie_header);
        std::string token;

        size_t session_pos = cookies.find("tsto_session=");
        if (session_pos != std::string::npos) {
            size_t start = session_pos + 13;    
            size_t end = cookies.find(';', start);

            if (end != std::string::npos) {
                token = cookies.substr(start, end - start);
            }
            else {
                token = cookies.substr(start);
            }
        }

        if (token.empty()) {
            return false;
        }

        std::string username;
        if (!UserRole::ValidateSession(token, username)) {
            return false;
        }

        RoleType role = UserRole::GetRoleByUsername(username);
        return role == RoleType::ADMIN;
    }

    bool Dashboard::has_town_operations_access(const evpp::http::ContextPtr& ctx) {
        const char* cookie_header = ctx->FindRequestHeader("Cookie");
        if (!cookie_header) {
            return false;
        }

        std::string cookies(cookie_header);
        std::string token;

        size_t session_pos = cookies.find("tsto_session=");
        if (session_pos != std::string::npos) {
            size_t start = session_pos + 13;    
            size_t end = cookies.find(';', start);

            if (end != std::string::npos) {
                token = cookies.substr(start, end - start);
            }
            else {
                token = cookies.substr(start);
            }
        }

        if (token.empty()) {
            return false;
        }

        std::string username;
        if (!UserRole::ValidateSession(token, username)) {
            return false;
        }

        RoleType role = UserRole::GetRoleByUsername(username);
        return (role == RoleType::ADMIN || role == RoleType::TOWN_OPERATOR);
    }

    bool Dashboard::has_currency_editor_access(const evpp::http::ContextPtr& ctx) {
        const char* cookie_header = ctx->FindRequestHeader("Cookie");
        if (!cookie_header) {
            return false;
        }

        std::string cookies(cookie_header);
        std::string token;

        size_t session_pos = cookies.find("tsto_session=");
        if (session_pos != std::string::npos) {
            size_t start = session_pos + 13;    
            size_t end = cookies.find(';', start);

            if (end != std::string::npos) {
                token = cookies.substr(start, end - start);
            }
            else {
                token = cookies.substr(start);
            }
        }

        if (token.empty()) {
            return false;
        }

        std::string username;
        if (!UserRole::ValidateSession(token, username)) {
            return false;
        }

        RoleType role = UserRole::GetRoleByUsername(username);
        return (role == RoleType::ADMIN || role == RoleType::TOWN_OPERATOR);
    }

    void Dashboard::redirect_to_login(const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
            "Redirecting to login page from URI: %s", ctx->uri().c_str());

        ctx->set_response_http_code(302);  

        ctx->AddResponseHeader("Location", "/login");

        ctx->AddResponseHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        ctx->AddResponseHeader("Pragma", "no-cache");
        ctx->AddResponseHeader("Expires", "0");

        ctx->AddResponseHeader("Content-Type", "text/html; charset=utf-8");

        cb("<html><head><meta http-equiv=\"refresh\" content=\"0;URL='/login'\"></head>"
            "<body>Redirecting to <a href=\"/login\">login page</a>...</body></html>");
    }

    void Dashboard::redirect_to_access_denied(const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
            "Redirecting to access denied page from URI: %s", ctx->uri().c_str());

        ctx->set_response_http_code(302);  
        ctx->AddResponseHeader("Location", "/access_denied.html");
        ctx->AddResponseHeader("Cache-Control", "no-cache, no-store, must-revalidate");
        ctx->AddResponseHeader("Pragma", "no-cache");
        ctx->AddResponseHeader("Expires", "0");

        cb("<html><head><meta http-equiv=\"refresh\" content=\"0;URL='/access_denied.html'\" /></head>"
            "<body>Redirecting to <a href=\"/access_denied.html\">access denied page</a>...</body></html>");
    }

    void Dashboard::handle_login_page(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (is_authenticated(ctx)) {
            const char* requested_with = ctx->FindRequestHeader("X-Requested-With");
            if (requested_with && std::string(requested_with) == "XMLHttpRequest") {
                ctx->AddResponseHeader("Content-Type", "application/json; charset=utf-8");
                cb("{\"authenticated\": true}");
                return;
            }

            ctx->set_response_http_code(302);  
            ctx->AddResponseHeader("Location", "/dashboard.html");
            cb("");
            return;
        }

        try {
            std::string login_html;
            std::ifstream file("webpanel/login.html");
            if (file.is_open()) {
                std::stringstream buffer;
                buffer << file.rdbuf();
                login_html = buffer.str();
                file.close();
            }
            else {
                throw std::runtime_error("Failed to open login.html");
            }

            ctx->AddResponseHeader("Content-Type", "text/html; charset=utf-8");

            std::string uri = std::string(ctx->uri());
            std::string error_message = "";
            if (uri.find("error=invalid") != std::string::npos) {
                error_message = "Invalid username or password";
            }

            if (!error_message.empty()) {
                size_t pos = login_html.find("id=\"errorMessage\" class=\"error-message\"");
                if (pos != std::string::npos) {
                    size_t style_pos = login_html.find("style=\"display: none\"", pos);
                    if (style_pos != std::string::npos) {
                        login_html.replace(style_pos, 22, "style=\"display: block\"");
                    }
                }
            }

            cb(login_html);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Login page error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("Error: Internal server error");
        }
    }

    void Dashboard::handle_login(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string body = ctx->body().ToString();

            std::string password;
            size_t password_pos = body.find("password=");
            if (password_pos != std::string::npos) {
                password = body.substr(password_pos + 9);    

                size_t end = password.find('&');
                if (end != std::string::npos) {
                    password = password.substr(0, end);
                }

                std::string decoded_password;
                for (size_t i = 0; i < password.length(); ++i) {
                    if (password[i] == '+') {
                        decoded_password += ' ';
                    }
                    else if (password[i] == '%' && i + 2 < password.length()) {
                        int value;
                        std::istringstream iss(password.substr(i + 1, 2));
                        if (iss >> std::hex >> value) {
                            decoded_password += static_cast<char>(value);
                            i += 2;
                        }
                        else {
                            decoded_password += password[i];
                        }
                    }
                    else {
                        decoded_password += password[i];
                    }
                }

                password = decoded_password;
            }

            std::string username;
            size_t username_pos = body.find("username=");
            if (username_pos != std::string::npos) {
                username = body.substr(username_pos + 9);    

                size_t end = username.find('&');
                if (end != std::string::npos) {
                    username = username.substr(0, end);
                }

                std::string decoded_username;
                for (size_t i = 0; i < username.length(); ++i) {
                    if (username[i] == '+') {
                        decoded_username += ' ';
                    }
                    else if (username[i] == '%' && i + 2 < username.length()) {
                        int value;
                        std::istringstream iss(username.substr(i + 1, 2));
                        if (iss >> std::hex >> value) {
                            decoded_username += static_cast<char>(value);
                            i += 2;
                        }
                        else {
                            decoded_username += username[i];
                        }
                    }
                    else {
                        decoded_username += username[i];
                    }
                }

                username = decoded_username;
            }

            bool authenticated = UserRole::AuthenticateUser(username, password);

            if (authenticated) {
                RoleType role = UserRole::GetRoleByUsername(username);

                std::string ip_address = std::string(ctx->remote_ip());
                std::string user_agent = "";
                const char* ua_header = ctx->FindRequestHeader("User-Agent");
                if (ua_header) {
                    user_agent = std::string(ua_header);
                }

                std::string token = UserRole::CreateSession(username, ip_address, user_agent, 24);    

                std::string cookie = "tsto_session=" + token + "; Path=/; Max-Age=86400; HttpOnly";     
                ctx->AddResponseHeader("Set-Cookie", cookie);

                ctx->set_response_http_code(302);  
                if (role == RoleType::ADMIN) {
                    ctx->AddResponseHeader("Location", "/dashboard.html");
                }
                else {
                    ctx->AddResponseHeader("Location", "/town_operations");
                }

                cb("");
            }
            else {
                ctx->set_response_http_code(302);  
                ctx->AddResponseHeader("Location", "/login?error=invalid");
                cb("");
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Login error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("Error: Internal server error");
        }
    }

    void Dashboard::handle_logout(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            const char* cookie_header = ctx->FindRequestHeader("Cookie");
            if (cookie_header) {
                std::string cookies(cookie_header);
                std::string token;

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                    "Logout request received with cookies: %s", cookies.c_str());

                size_t session_pos = cookies.find("tsto_session=");
                if (session_pos != std::string::npos) {
                    size_t start = session_pos + 13;    
                    size_t end = cookies.find(';', start);

                    if (end != std::string::npos) {
                        token = cookies.substr(start, end - start);
                    }
                    else {
                        token = cookies.substr(start);
                    }

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                        "Attempting to invalidate session token during logout: %s", token.c_str());

                    bool success = UserRole::InvalidateSession(token);

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                        "Session invalidation result: %s", success ? "success" : "failed");
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                        "No session token found in cookies during logout");
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                    "No cookies found in logout request");
            }

            std::string cookie = "tsto_session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly";
            ctx->AddResponseHeader("Set-Cookie", cookie);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
                "Session cookie cleared, redirecting to login page");

            ctx->set_response_http_code(302);  
            ctx->AddResponseHeader("Location", "/login.html");
            cb("");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Logout error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("Error: Internal server error");
        }
    }

    void Dashboard::handle_api_login(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string ip_address = std::string(ctx->remote_ip());

            auto& security = tsto::security::LoginSecurity::get_instance();
            if (!security.can_attempt_login(ip_address)) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "Too many failed login attempts. Please try again later.", response.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(429);    
                cb(buffer.GetString());
                return;
            }

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("username") || !doc.HasMember("password")) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "Invalid request format", response.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(400);
                cb(buffer.GetString());
                return;
            }

            std::string username = doc["username"].GetString();
            std::string password = doc["password"].GetString();

            if (tsto::security::is_suspicious_input(username) || tsto::security::is_suspicious_input(password)) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "Invalid input detected", response.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(400);
                cb(buffer.GetString());

                security.record_failed_attempt(ip_address, username);
                return;
            }

            if (security.is_account_locked(username)) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "Account is temporarily locked due to too many failed attempts. Please try again later.", response.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(403);  
                cb(buffer.GetString());
                return;
            }

            bool authenticated = UserRole::AuthenticateUser(username, password);

            if (authenticated) {
                security.record_successful_login(ip_address, username);

                RoleType role = UserRole::GetRoleByUsername(username);

                std::string user_agent = "";
                const char* ua_header = ctx->FindRequestHeader("User-Agent");
                if (ua_header) {
                    user_agent = std::string(ua_header);
                }

                std::string token = UserRole::CreateSession(username, ip_address, user_agent, 24);    

                std::string cookie = "tsto_session=" + token + "; Path=/; Max-Age=86400; HttpOnly; SameSite=Strict";     
                ctx->AddResponseHeader("Set-Cookie", cookie);

                rapidjson::Document response;
                response.SetObject();
                auto& allocator = response.GetAllocator();

                response.AddMember("success", true, response.GetAllocator());
                response.AddMember("token", rapidjson::Value(token.c_str(), response.GetAllocator()), response.GetAllocator());

                std::string role_str = UserRole::RoleTypeToString(role);
                response.AddMember("role", rapidjson::Value(role_str.c_str(), response.GetAllocator()), response.GetAllocator());

                response.AddMember("username", rapidjson::Value(username.c_str(), response.GetAllocator()), response.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(200);
                cb(buffer.GetString());
            }
            else {
                security.record_failed_attempt(ip_address, username);

                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "Invalid username or password", response.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(401);
                cb(buffer.GetString());
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "API login error: %s", ex.what());

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Internal server error", response.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->set_response_http_code(500);
            cb(buffer.GetString());
        }
    }

    void Dashboard::handle_api_validate_session(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("token")) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("valid", false, response.GetAllocator());
                response.AddMember("error", "Invalid request format", response.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(400);
                cb(buffer.GetString());
                return;
            }

            std::string token = doc["token"].GetString();
            std::string username;

            bool valid = UserRole::ValidateSession(token, username);

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("valid", valid, response.GetAllocator());

            if (valid) {
                RoleType role = UserRole::GetRoleByUsername(username);
                std::string role_str = UserRole::RoleTypeToString(role);

                response.AddMember("username", rapidjson::Value(username.c_str(), response.GetAllocator()), response.GetAllocator());
                response.AddMember("role", rapidjson::Value(role_str.c_str(), response.GetAllocator()), response.GetAllocator());
            }

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->set_response_http_code(200);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "API validate session error: %s", ex.what());

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("valid", false, response.GetAllocator());
            response.AddMember("error", "Internal server error", response.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->set_response_http_code(500);
            cb(buffer.GetString());
        }
    }

    void Dashboard::handle_api_logout(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("token")) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "Invalid request format", response.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(400);
                cb(buffer.GetString());
                return;
            }

            std::string token = doc["token"].GetString();

            bool success = UserRole::InvalidateSession(token);

            std::string cookie = "tsto_session=; Path=/; Expires=Thu, 01 Jan 1970 00:00:00 GMT; HttpOnly";
            ctx->AddResponseHeader("Set-Cookie", cookie);

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", success, response.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->set_response_http_code(200);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "API logout error: %s", ex.what());

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Internal server error", response.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->set_response_http_code(500);
            cb(buffer.GetString());
        }
    }

    void Dashboard::handle_validate_session(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        ctx->AddResponseHeader("Content-Type", "application/json");

        if (!is_authenticated(ctx)) {
            cb("{\"success\": false, \"message\": \"Not authenticated\"}");
            return;
        }

        std::string username = get_username_from_session(ctx);

        std::string response = "{\"success\": true, \"username\": \"" + username + "\"}";
        cb(response);
    }

    std::string Dashboard::get_username_from_session(const evpp::http::ContextPtr& ctx) {
        const char* cookie_header = ctx->FindRequestHeader("Cookie");
        if (!cookie_header) {
            return "";
        }

        std::string cookies(cookie_header);
        std::string token;

        size_t session_pos = cookies.find("tsto_session=");
        if (session_pos != std::string::npos) {
            size_t start = session_pos + 13;    
            size_t end = cookies.find(';', start);

            if (end != std::string::npos) {
                token = cookies.substr(start, end - start);
            }
            else {
                token = cookies.substr(start);
            }
        }
        else {
            return "";
        }

        std::string username;
        bool is_valid = UserRole::ValidateSession(token, username);

        return is_valid ? username : "";
    }

    void Dashboard::handle_dashboard(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "text/html; charset=utf-8");

        try {
            std::filesystem::path templatePath = "webpanel/dashboard.html";

            if (!std::filesystem::exists(templatePath)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Dashboard template not found at %s", templatePath.string().c_str());
                cb("Error: Dashboard template file not found");
                return;
            }

            std::ifstream file(templatePath, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to open dashboard template: %s", templatePath.string().c_str());
                cb("Error: Failed to open dashboard template");
                return;
            }

            std::stringstream template_stream;
            template_stream << file.rdbuf();
            std::string html_template = template_stream.str();

            html_template = std::regex_replace(html_template, std::regex("%SERVER_IP%"), server_ip_);
            html_template = std::regex_replace(html_template, std::regex("\\{\\{ GAME_PORT \\}\\}"), std::to_string(server_port_));

            extern std::chrono::steady_clock::time_point g_server_start_time;
            auto now = std::chrono::steady_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - ::g_server_start_time);
            auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime % std::chrono::hours(1));
            auto seconds = uptime % std::chrono::minutes(1);
            std::stringstream uptime_str;
            uptime_str << hours.count() << "h " << minutes.count() << "m " << seconds.count() << "s";
            html_template = std::regex_replace(html_template, std::regex("%UPTIME%"), uptime_str.str());

            std::string currency_path = "towns/currency.txt";
            std::string dlc_directory = utils::configuration::ReadString("Server", "DLCDirectory", "dlc");
            html_template = std::regex_replace(html_template, std::regex("%DLC_DIRECTORY%"), dlc_directory);

            html_template = std::regex_replace(html_template, std::regex("%INITIAL_DONUTS%"),
                utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));

            auto current_event = tsto::events::Events::get_current_event();
            html_template = std::regex_replace(html_template, std::regex("%CURRENT_EVENT%"), current_event.name);

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
                "Dashboard error: %s", ex.what());
            cb("Error: Internal server error");
        }
    }

    void Dashboard::handle_server_restart(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "application/json");

        cb("{\"status\":\"success\",\"message\":\"Server restart initiated\"}");

        loop->RunAfter(evpp::Duration(1.0), []() {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Server stopping for restart...");

            char exePath[MAX_PATH];
            GetModuleFileNameA(NULL, exePath, MAX_PATH);

            STARTUPINFOA si = { 0 };
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
                    "Failed to restart server: %d", GetLastError());
            }
            });
    }

    void Dashboard::handle_server_stop(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "application/json");

        cb("{\"status\":\"success\",\"message\":\"Server shutdown initiated\"}");

        loop->RunAfter(evpp::Duration(1.0), []() {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Server stopping...");
            ExitProcess(0);
            });
    }



    void Dashboard::handle_force_save_protoland(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            auto& session = tsto::Session::get();

            if (!session.land_proto.IsInitialized()) {
                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(400);
                cb("{\"status\":\"error\",\"message\":\"No land data to save\"}");
                return;
            }

            else {
                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(500);
                cb("{\"status\":\"error\",\"message\":\"Failed to save town\"}");
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to force save town");
            }
        }
        catch (const std::exception& ex) {
            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->set_response_http_code(500);
            cb("{\"status\":\"error\",\"message\":\"" + std::string(ex.what()) + "\"}");
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Error during force save: %s", ex.what());
        }
    }

    void Dashboard::handle_update_dlc_directory(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.HasMember("dlcDirectory") || !doc["dlcDirectory"].IsString()) {
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid request body\"}");
                return;
            }

            std::string dlc_directory = doc["dlcDirectory"].GetString();
            utils::configuration::WriteString("Server", "DLCDirectory", dlc_directory);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                "[DLC] Updated DLC directory to: %s", dlc_directory.c_str());

            headers::set_json_response(ctx);
            cb("{\"success\": true}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                "[DLC] Error updating DLC directory: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\": \"Internal server error\"}");
        }
    }

    void Dashboard::handle_update_server_ip(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.HasMember("serverIp") || !doc["serverIp"].IsString()) {
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid request body\"}");
                return;
            }

            std::string server_ip = doc["serverIp"].GetString();
            utils::configuration::WriteString("ServerConfig", "ServerIP", server_ip);
            utils::configuration::WriteBoolean("ServerConfig", "AutoDetectIP", false);
            server_ip_ = server_ip;

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "[CONFIG] Updated server IP to: %s", server_ip.c_str());

            headers::set_json_response(ctx);
            cb("{\"success\": true}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "[CONFIG] Error updating server IP: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\": \"Internal server error\"}");
        }
    }

    void Dashboard::handle_update_server_port(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.HasMember("serverPort") || !doc["serverPort"].IsInt()) {
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid request body\"}");
                return;
            }

            int server_port = doc["serverPort"].GetInt();
            utils::configuration::WriteUnsignedInteger("ServerConfig", "GamePort", static_cast<unsigned int>(server_port));
            server_port_ = static_cast<uint16_t>(server_port);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "[CONFIG] Updated server port to: %d", server_port);

            headers::set_json_response(ctx);
            cb("{\"success\": true,\"message\":\"Server port updated. Please restart the server for changes to take effect.\"}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "[CONFIG] Error updating server port: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\": \"Internal server error\"}");
        }
    }

    void Dashboard::handle_browse_directory(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            BROWSEINFO bi = { 0 };
            bi.lpszTitle = "Select DLC Directory";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

            LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
            if (pidl != nullptr) {
                char path[MAX_PATH];
                if (SHGetPathFromIDList(pidl, path)) {
                    CoTaskMemFree(pidl);

                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    writer.StartObject();
                    writer.Key("success");
                    writer.Bool(true);
                    writer.Key("path");
                    writer.String(path);
                    writer.EndObject();

                    headers::set_json_response(ctx);
                    cb(buffer.GetString());
                    return;
                }
                CoTaskMemFree(pidl);
            }

            headers::set_json_response(ctx);
            cb("{\"success\":false,\"error\":\"No directory selected\"}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "[CONFIG] Error browsing directory: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\": \"Internal server error\"}");
        }
    }

    void Dashboard::handle_edit_user_currency(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!has_currency_editor_access(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc.HasMember("amount") ||
                !doc["email"].IsString() || !doc["amount"].IsInt()) {

                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid request format\"}");
                return;
            }

            std::string email = doc["email"].GetString();
            int amount = doc["amount"].GetInt();

            std::string display_name;
            if (doc.HasMember("displayName") && doc["displayName"].IsString()) {
                display_name = doc["displayName"].GetString();

                if (!display_name.empty()) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Updating display name for user %s to: %s",
                        email.c_str(), display_name.c_str());

                    database::Database::get_instance().update_display_name(email, display_name);
                }
            }

            size_t pb_pos = email.find(".pb");
            if (pb_pos != std::string::npos) {
                email = email.substr(0, pb_pos);
            }
            size_t txt_pos = email.find(".txt");
            if (txt_pos != std::string::npos) {
                email = email.substr(0, txt_pos);
            }

            bool is_anonymous = email.find('@') == std::string::npos;
            bool use_legacy_mode = utils::configuration::ReadBoolean("Land", "UseLegacyMode", false);

            std::string currency_file;
            std::string town_file;

            if (is_anonymous && use_legacy_mode) {
                currency_file = "towns/currency.txt";
                town_file = "towns/mytown.pb";
            }
            else if (email == "mytown") {
                currency_file = "towns/currency.txt";
                town_file = "towns/mytown.pb";
            }
            else if (is_anonymous && !use_legacy_mode && email.find("anon_") == 0) {
                currency_file = "towns/" + email + ".txt";
                town_file = "towns/" + email + ".pb";
            }
            else {
                currency_file = "towns/" + email + ".txt";
                town_file = "towns/" + email + ".pb";
            }

            std::filesystem::create_directories("towns");

            if (amount <= 0) {
                amount = std::stoi(utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));
            }

            std::ofstream output(currency_file);
            if (!output.good()) {
                throw std::runtime_error("Failed to open currency file for writing");
            }
            output << amount;
            output.close();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[CURRENCY] Updated currency for user %s to %d donuts (file: %s, town: %s)",
                email.c_str(), amount, currency_file.c_str(), town_file.c_str());

            rapidjson::Document response;
            response.SetObject();
            auto& allocator = response.GetAllocator();

            response.AddMember("status", "success", response.GetAllocator());
            response.AddMember("message", "Currency updated successfully", response.GetAllocator());
            response.AddMember("amount", amount, response.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            headers::set_json_response(ctx);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[CURRENCY] Error updating currency: %s", ex.what());

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("status", "error", response.GetAllocator());
            response.AddMember("message", rapidjson::StringRef(ex.what()), response.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->set_response_http_code(500);
            headers::set_json_response(ctx);
            cb(buffer.GetString());
        }
    }

    void Dashboard::handle_upload_town_file(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!has_town_operations_access(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[UPLOAD] Received town file upload request from: %s", std::string(ctx->remote_ip()).c_str());

            std::string content_type = ctx->FindRequestHeader("Content-Type");
            if (content_type.find("multipart/form-data") == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Invalid content type: %s", content_type.c_str());
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"error\":\"Invalid content type. Expected multipart/form-data\"}");
                return;
            }

            const char* auth_header = ctx->FindRequestHeader("nucleus_token");
            if (auth_header && strlen(auth_header) > 0) {
                auto& db = tsto::database::Database::get_instance();
                std::string token_email;

                if (db.validate_access_token(auth_header, token_email)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[UPLOAD] Valid token for user: %s", token_email.c_str());
                }
                else {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
                        "[UPLOAD] Invalid token provided");
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[UPLOAD] No authentication token provided");
            }

            std::filesystem::create_directories("temp");

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 15);
            std::uniform_int_distribution<> dis2(8, 11);

            std::stringstream ss;
            ss << "temp/upload_" << std::time(nullptr) << "_";
            for (int i = 0; i < 16; i++) {
                ss << std::hex << dis(gen);
            }
            ss << ".pb";

            std::string temp_file_path = ss.str();

            auto& body = ctx->body();

            std::string boundary;
            size_t boundary_pos = content_type.find("boundary=");
            if (boundary_pos != std::string::npos) {
                boundary = content_type.substr(boundary_pos + 9);
                if (boundary.front() == '"' && boundary.back() == '"') {
                    boundary = boundary.substr(1, boundary.length() - 2);
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] No boundary found in content type");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"error\":\"Invalid multipart form data format\"}");
                return;
            }

            std::string full_boundary = "--" + boundary;
            std::string body_str = body.ToString();

            size_t pos = body_str.find(full_boundary);
            if (pos == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Could not find boundary in request body");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"error\":\"Invalid multipart form data format\"}");
                return;
            }

            pos = body_str.find("Content-Disposition:", pos);
            if (pos == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Could not find Content-Disposition header");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"error\":\"Invalid multipart form data format\"}");
                return;
            }

            size_t headers_end = body_str.find("\r\n\r\n", pos);
            if (headers_end == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Could not find end of headers");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"error\":\"Invalid multipart form data format\"}");
                return;
            }

            size_t data_start = headers_end + 4;

            size_t data_end = body_str.find(full_boundary, data_start);
            if (data_end == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Could not find end boundary");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"error\":\"Invalid multipart form data format\"}");
                return;
            }

            if (data_end > 2) {
                data_end -= 2;
            }

            std::string file_data = body_str.substr(data_start, data_end - data_start);

            std::ofstream out_file(temp_file_path);
            if (!out_file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Failed to open temporary file for writing: %s", temp_file_path.c_str());
                ctx->set_response_http_code(500);
                headers::set_json_response(ctx);
                cb("{\"error\":\"Failed to save uploaded file\"}");
                return;
            }

            out_file.write(file_data.c_str(), file_data.size());
            out_file.close();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[UPLOAD] Successfully saved uploaded town file to: %s", temp_file_path.c_str());

            headers::set_json_response(ctx);
            cb("{\"success\":true,\"message\":\"File uploaded successfully\",\"filePath\":\"" + temp_file_path + "\"}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[UPLOAD] Exception handling town file upload: %s", ex.what());
            ctx->set_response_http_code(500);
            headers::set_json_response(ctx);
            cb("{\"error\":\"Internal server error: " + std::string(ex.what()) + "\"}");
        }
    }

    void Dashboard::handle_list_users(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!has_currency_editor_access(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            std::string users_file = "users.json";
            rapidjson::Document users_doc;

            if (!std::filesystem::exists(users_file)) {
                std::ofstream file(users_file);
                file << "{\"users\": []}" << std::endl;
                file.close();
            }

            std::ifstream file(users_file);
            std::string json_str((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if (users_doc.Parse(json_str.c_str()).HasParseError()) {
                throw std::runtime_error("Failed to parse users.json");
            }

            std::vector<std::string> town_files;
            for (const auto& entry : std::filesystem::directory_iterator("towns")) {
                if (entry.path().extension() == ".pb") {
                    town_files.push_back(entry.path().filename().string());
                }
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();

            response.AddMember("success", true, response.GetAllocator());

            std::string current_username = get_username_from_session(ctx);
            bool is_admin_user = is_admin(ctx);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[USER_API] Current user: %s, Is admin: %s",
                current_username.c_str(), is_admin_user ? "true" : "false");

            rapidjson::Value users_array(rapidjson::kArrayType);
            for (const auto& town_file : town_files) {
                rapidjson::Value user_obj(rapidjson::kObjectType);

                std::string username = town_file;
                size_t pos = username.find(".pb");
                if (pos != std::string::npos) {
                    username = username.substr(0, pos);
                }

                std::string currency_file;

                bool is_anonymous = username.find('@') == std::string::npos;
                bool use_legacy_mode = utils::configuration::ReadBoolean("Land", "UseLegacyMode", false);

                if (is_anonymous && use_legacy_mode) {
                    currency_file = "towns/currency.txt";
                }
                else if (username == "mytown") {
                    currency_file = "towns/currency.txt";
                }
                else if (is_anonymous && !use_legacy_mode && username.find("anon_") == 0) {
                    currency_file = "towns/currency_" + username + ".txt";
                }
                else {
                    currency_file = "towns/currency_" + username + ".txt";
                }

                int currency = 0;
                if (std::filesystem::exists(currency_file)) {
                    std::ifstream curr_file(currency_file);
                    curr_file >> currency;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Read currency for user %s: %d donuts (file: %s)",
                        username.c_str(), currency, currency_file.c_str());
                }
                else {
                    std::string alt_currency_file = "towns/" + username + ".txt";
                    if (std::filesystem::exists(alt_currency_file)) {
                        std::ifstream curr_file(alt_currency_file);
                        curr_file >> currency;
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                            "[CURRENCY] Read currency for user %s from alternate file: %d donuts (file: %s)",
                            username.c_str(), currency, alt_currency_file.c_str());
                    }
                }

                user_obj.AddMember("username", rapidjson::Value(username.c_str(), allocator), allocator);
                user_obj.AddMember("townFile", rapidjson::Value(town_file.c_str(), allocator), allocator);
                user_obj.AddMember("currency", currency, allocator);
                user_obj.AddMember("isLegacy", username == "mytown", allocator);

                std::string town_path = "towns/" + town_file;
                if (std::filesystem::exists(town_path)) {
                    std::uintmax_t file_size = std::filesystem::file_size(town_path);
                    user_obj.AddMember("landSize", static_cast<uint64_t>(file_size), allocator);

                    std::string formatted_size;
                    if (file_size < 1024) {
                        formatted_size = std::to_string(file_size) + " B";
                    }
                    else if (file_size < 1024 * 1024) {
                        formatted_size = std::to_string(file_size / 1024) + " KB";
                    }
                    else if (file_size < 1024 * 1024 * 1024) {
                        formatted_size = std::to_string(file_size / (1024 * 1024)) + " MB";
                    }
                    else {
                        formatted_size = std::to_string(file_size / (1024 * 1024 * 1024)) + " GB";
                    }
                    user_obj.AddMember("landSizeFormatted", rapidjson::Value(formatted_size.c_str(), allocator), allocator);

                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[USER_API] Added land size for user %s: %s (%llu bytes)",
                        username.c_str(), formatted_size.c_str(), static_cast<unsigned long long>(file_size));
                }

                std::string display_name;
                if (database::Database::get_instance().get_display_name(username, display_name)) {
                    user_obj.AddMember("displayName", rapidjson::Value(display_name.c_str(), allocator), allocator);
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[USER_API] Added display name for user %s: %s",
                        username.c_str(), display_name.c_str());
                }
                else {
                    std::string email;
                    if (database::Database::get_instance().get_email_by_user_id(username, email) &&
                        !email.empty() &&
                        database::Database::get_instance().get_display_name(email, display_name)) {
                        user_obj.AddMember("displayName", rapidjson::Value(display_name.c_str(), allocator), allocator);
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                            "[USER_API] Added display name for user %s via email %s: %s",
                            username.c_str(), email.c_str(), display_name.c_str());
                    }
                }

                users_array.PushBack(user_obj, allocator);
            }

            response.AddMember("users", users_array, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "[DASHBOARD] Error listing users: %s", ex.what());
            cb("{\"error\": \"Failed to list users\"}");
        }
    }

    void Dashboard::handle_get_user_save(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!has_town_operations_access(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "Handling get-user-save request");

            std::string requestBody = std::string(ctx->body().data(), ctx->body().size());
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "Request body: %s",
                requestBody.c_str());

            std::string username;
            bool isLegacy = false;

            if (!requestBody.empty()) {
                rapidjson::Document request;
                rapidjson::ParseResult parseResult = request.Parse(requestBody.c_str());
                if (parseResult.IsError()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                        "Failed to parse request JSON: error code %d at offset %zu",
                        parseResult.Code(), parseResult.Offset());
                    ctx->set_response_http_code(400);
                    cb("{\"error\":\"Invalid JSON in request\"}");
                    return;
                }

                if (!request.HasMember("username") || !request["username"].IsString()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Missing username in request");
                    ctx->set_response_http_code(400);
                    cb("{\"error\":\"Missing username\"}");
                    return;
                }

                username = request["username"].GetString();
                isLegacy = request.HasMember("isLegacy") && request["isLegacy"].IsBool() && request["isLegacy"].GetBool();
            }
            else {
                const std::string& uri = ctx->original_uri();
                size_t pos = uri.find("username=");
                if (pos == std::string::npos) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Missing username parameter");
                    ctx->set_response_http_code(400);
                    cb("{\"error\":\"Missing username parameter\"}");
                    return;
                }

                username = uri.substr(pos + 9);
                pos = username.find('&');
                if (pos != std::string::npos) {
                    username = username.substr(0, pos);
                }

                std::string decoded;
                decoded.reserve(username.length());

                for (size_t i = 0; i < username.length(); ++i) {
                    if (username[i] == '%' && i + 2 < username.length()) {
                        int value;
                        std::istringstream iss(username.substr(i + 1, 2));
                        if (iss >> std::hex >> value) {
                            decoded += static_cast<char>(value);
                            i += 2;
                        }
                        else {
                            decoded += username[i];
                        }
                    }
                    else if (username[i] == '+') {
                        decoded += ' ';
                    }
                    else {
                        decoded += username[i];
                    }
                }

                username = decoded;
                isLegacy = (username == "mytown");
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Loading save for user: %s%s", username.c_str(), isLegacy ? " (legacy)" : "");

            try {
                std::filesystem::path townsDir = std::filesystem::current_path() / "towns";

                if (!std::filesystem::exists(townsDir)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                        "Creating towns directory: %s", townsDir.string().c_str());
                    if (!std::filesystem::create_directories(townsDir)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                            "Failed to create towns directory: %s", townsDir.string().c_str());
                        ctx->set_response_http_code(500);
                        cb("{\"error\":\"Failed to create towns directory\"}");
                        return;
                    }
                }

                std::filesystem::path savePath = townsDir;
                if (isLegacy) {
                    savePath /= "mytown.pb";
                }
                else {
                    savePath /= (username + ".pb");
                }

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Attempting to read save file: %s", savePath.string().c_str());

                if (!std::filesystem::exists(savePath)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                        "Save file does not exist: %s", savePath.string().c_str());
                    ctx->set_response_http_code(404);
                    cb("{\"error\":\"Save file not found\"}");
                    return;
                }

                std::ifstream in(savePath, std::ios::binary);
                if (!in.is_open()) {
                    throw std::runtime_error("Failed to open save file for reading");
                }

                Data::LandMessage save_data;
                if (!save_data.ParseFromIstream(&in)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                        "Failed to parse save file for user: %s", username.c_str());
                    ctx->set_response_http_code(500);
                    cb("{\"error\":\"Failed to parse save file\"}");
                    return;
                }

                std::string json_string;
                google::protobuf::util::JsonPrintOptions options;
                options.add_whitespace = true;
                options.preserve_proto_field_names = true;

                auto status = google::protobuf::util::MessageToJsonString(save_data, &json_string, options);
                if (!status.ok()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                        "Failed to convert save to JSON for user: %s - %s", username.c_str(), status.ToString().c_str());
                    ctx->set_response_http_code(500);
                    cb("{\"error\":\"Failed to convert save to JSON\"}");
                    return;
                }

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Successfully loaded save file for user: %s", username.c_str());

                rapidjson::Document response;
                response.SetObject();
                auto& allocator = response.GetAllocator();

                response.AddMember("status", "success", allocator);
                response.AddMember("save", rapidjson::Value(json_string.c_str(), allocator), allocator);

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->set_response_http_code(200);
                cb(buffer.GetString());
            }
            catch (const std::exception& e) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Exception while loading save: %s", e.what());
                ctx->set_response_http_code(500);
                cb(std::string("{\"error\": \"Failed to read save file: ") + e.what() + "\"}");
            }
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Exception in handle_get_user_save: %s", e.what());
            ctx->set_response_http_code(500);
            cb(std::string("{\"error\": \"Server error: ") + e.what() + "\"}");
        }
    }

    void Dashboard::handle_save_user_save(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!has_town_operations_access(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            std::string requestBody = std::string(ctx->body().data(), ctx->body().size());
            rapidjson::Document request;
            rapidjson::ParseResult parseResult = request.Parse(requestBody.c_str());
            if (parseResult.IsError()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Failed to parse request JSON: error code %d at offset %zu",
                    parseResult.Code(), parseResult.Offset());
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid JSON in request\"}");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Request has fields - username: %d, isLegacy: %d, save: %d",
                request.HasMember("username"), request.HasMember("isLegacy"), request.HasMember("save"));

            if (!request.HasMember("username") || !request.HasMember("isLegacy") || !request.HasMember("save")) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Missing required fields in request");
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Missing required fields\"}");
                return;
            }

            if (!request["username"].IsString() || !request["isLegacy"].IsBool() || !request["save"].IsString()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Invalid field types in request");
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid field types\"}");
                return;
            }

            const std::string username = request["username"].GetString();
            const bool isLegacy = request["isLegacy"].GetBool();
            const std::string json_string = request["save"].GetString();

            try {
                Data::LandMessage save_data;

                auto status = google::protobuf::util::JsonStringToMessage(json_string, &save_data);
                if (!status.ok()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                        "Failed to parse JSON for user: %s - %s", username.c_str(), status.ToString().c_str());
                    ctx->set_response_http_code(400);
                    cb("{\"error\":\"Invalid save data format\"}");
                    return;
                }

                std::filesystem::path savePath = std::filesystem::current_path() / "towns";
                if (!std::filesystem::exists(savePath)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                        "Creating towns directory: %s", savePath.string().c_str());
                    if (!std::filesystem::create_directories(savePath)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                            "Failed to create towns directory: %s", savePath.string().c_str());
                        ctx->set_response_http_code(500);
                        cb("{\"error\":\"Failed to create towns directory\"}");
                        return;
                    }
                }

                if (isLegacy) {
                    savePath /= "mytown.pb";
                }
                else {
                    savePath /= (username + ".pb");
                }

                if (std::filesystem::exists(savePath)) {
                    std::filesystem::path backupPath = savePath;
                    backupPath += ".bak";
                    std::filesystem::copy_file(savePath, backupPath, std::filesystem::copy_options::overwrite_existing);
                }

                std::string serialized;
                if (!save_data.SerializeToString(&serialized)) {
                    throw std::runtime_error("Failed to serialize protobuf data");
                }

                std::vector<char> buffer(serialized.begin(), serialized.end());

                std::ofstream out(savePath, std::ios::binary);
                if (!out.is_open()) {
                    throw std::runtime_error("Failed to open save file for writing");
                }

                out.write(buffer.data(), buffer.size());
                out.close();

                std::ifstream verify(savePath, std::ios::binary);
                if (verify.is_open()) {
                    std::vector<char> check_buffer((std::istreambuf_iterator<char>(verify)), std::istreambuf_iterator<char>());
                    verify.close();

                    Data::LandMessage test_load;
                    if (!test_load.ParseFromArray(check_buffer.data(), static_cast<int>(check_buffer.size()))) {
                        throw std::runtime_error("Failed to verify saved protobuf data");
                    }
                }

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Successfully saved and verified file for user: %s", username.c_str());

                ctx->set_response_http_code(200);
                cb("{\"success\":true}");
            }
            catch (const std::exception& e) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Exception while saving: %s", e.what());
                ctx->set_response_http_code(500);
                cb(std::string("{\"error\": \"Failed to save file: ") + e.what() + "\"}");
            }
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Exception in handle_save_user_save: %s", e.what());
            ctx->set_response_http_code(500);
            cb(std::string("{\"error\": \"Server error: ") + e.what() + "\"}");
        }
    }

    void Dashboard::handle_dashboard_data(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "application/json");
        ctx->AddResponseHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        ctx->AddResponseHeader("Pragma", "no-cache");
        ctx->AddResponseHeader("Expires", "0");

        try {
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            doc.AddMember("server_ip", rapidjson::Value(server_ip_.c_str(), allocator), allocator);
            doc.AddMember("server_port", server_port_, allocator);

            extern std::chrono::steady_clock::time_point g_server_start_time;
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ::g_server_start_time);
            auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime % std::chrono::hours(1));
            auto seconds = uptime % std::chrono::minutes(1);
            std::stringstream uptime_str;
            uptime_str << hours.count() << "h " << minutes.count() << "m " << seconds.count() << "s";
            doc.AddMember("uptime", rapidjson::Value(uptime_str.str().c_str(), allocator), allocator);

            std::string dlc_directory = utils::configuration::ReadString("Server", "DLCDirectory", "dlc");
            doc.AddMember("dlc_directory", rapidjson::Value(dlc_directory.c_str(), allocator), allocator);

            std::string initial_donuts = utils::configuration::ReadString("Server", "InitialDonutAmount", "1000");
            doc.AddMember("initial_donuts", rapidjson::Value(initial_donuts.c_str(), allocator), allocator);

            auto current_event = tsto::events::Events::get_current_event();
            doc.AddMember("current_event", rapidjson::Value(current_event.name.c_str(), allocator), allocator);
            doc.AddMember("current_event_time", current_event.start_time, allocator);

            rapidjson::Value events_obj(rapidjson::kObjectType);
            for (const auto& event_pair : tsto::events::tsto_events) {
                rapidjson::Value key(std::to_string(event_pair.first).c_str(), allocator);
                rapidjson::Value value(event_pair.second.c_str(), allocator);
                events_obj.AddMember(key, value, allocator);
            }
            doc.AddMember("events", events_obj, allocator);

            int active_connections = tsto::statistics::Statistics::get_instance().get_active_connections();
            doc.AddMember("active_connections", active_connections, allocator);

            int unique_clients = tsto::statistics::Statistics::get_instance().get_unique_clients();
            doc.AddMember("unique_clients", unique_clients, allocator);
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[STATISTICS] Sending statistics: unique_clients=%d, active_connections=%d",
                unique_clients, active_connections);

            if (is_admin(ctx)) {
                auto town_upload_stats = UserRole::GetTownUploadStats();

                rapidjson::Value uploads_array(rapidjson::kArrayType);

                for (const auto& [username, total_uploads, last_upload] : town_upload_stats) {
                    rapidjson::Value user_obj(rapidjson::kObjectType);

                    user_obj.AddMember("username", rapidjson::Value(username.c_str(), allocator), allocator);

                    user_obj.AddMember("total_uploads", total_uploads, allocator);

                    user_obj.AddMember("last_upload", rapidjson::Value(last_upload.c_str(), allocator), allocator);

                    uploads_array.PushBack(user_obj, allocator);
                }

                doc.AddMember("upload_stats", uploads_array, allocator);
            }

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Dashboard data error: %s", ex.what());
            cb("{\"error\": \"Internal server error\"}");
        }
    }

    void Dashboard::handle_api_server_uptime(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        extern std::chrono::steady_clock::time_point g_server_start_time;
        auto uptime = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - ::g_server_start_time);

        rapidjson::Document doc;
        doc.SetObject();
        rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

        uint64_t uptime_seconds = static_cast<uint64_t>(uptime.count());
        rapidjson::Value uptime_value(uptime_seconds);
        doc.AddMember("uptime", uptime_value, allocator);

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        doc.Accept(writer);

        std::string response_body = buffer.GetString();
        ctx->AddResponseHeader("Content-Type", "application/json");
        ctx->AddResponseHeader("Access-Control-Allow-Origin", "*");

        cb(response_body);

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
            "Sent server uptime: %lld seconds", uptime.count());
    }

    void Dashboard::handle_game_config(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "text/html; charset=utf-8");

        try {
            std::filesystem::path templatePath = "webpanel/game_config.html";

            if (!std::filesystem::exists(templatePath)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Game config template not found at %s", templatePath.string().c_str());
                cb("Error: Game config template file not found");
                return;
            }

            std::ifstream file(templatePath, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to open game config template: %s", templatePath.string().c_str());
                cb("Error: Failed to open game config template");
                return;
            }

            std::stringstream template_stream;
            template_stream << file.rdbuf();
            std::string html_template = template_stream.str();

            cb(html_template);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Game config page error: %s", ex.what());
            cb("Error: Internal server error");
        }
    }

    void Dashboard::handle_user_management(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "text/html; charset=utf-8");

        try {
            std::filesystem::path templatePath = "webpanel/admin_management.html";

            if (!std::filesystem::exists(templatePath)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Admin management template not found at %s", templatePath.string().c_str());
                cb("Error: Admin management template file not found");
                return;
            }

            std::ifstream file(templatePath, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to open admin management template: %s", templatePath.string().c_str());
                cb("Error: Failed to open admin management template");
                return;
            }

            std::stringstream template_stream;
            template_stream << file.rdbuf();
            std::string html_template = template_stream.str();

            cb(html_template);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "User management page error: %s", ex.what());
            cb("Error: Internal server error");
        }
    }

    void Dashboard::handle_users_api(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        if (!is_authenticated(ctx)) {
            ctx->set_response_http_code(401);
            cb("{\"success\": false, \"message\": \"Authentication required\"}");
            return;
        }

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            const std::string method = ctx->GetMethod();

            if (method == "GET") {
                auto users = UserRole::GetAllUsers();

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[USER_API] Retrieved %d users from database", users.size());

                rapidjson::Document doc;
                doc.SetObject();
                rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

                doc.AddMember("success", true, allocator);

                std::string current_username = get_username_from_session(ctx);
                bool is_admin_user = is_admin(ctx);

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[USER_API] Current user: %s, Is admin: %s",
                    current_username.c_str(), is_admin_user ? "true" : "false");

                rapidjson::Value users_array(rapidjson::kArrayType);
                for (const auto& user_tuple : users) {
                    std::string username = std::get<0>(user_tuple);
                    RoleType role = std::get<1>(user_tuple);

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[USER_API] Processing user: %s, Role: %s",
                        username.c_str(), UserRole::RoleTypeToString(role).c_str());

                    if (!is_admin_user && username != current_username) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[USER_API] Skipping user %s (not current user)", username.c_str());
                        continue;
                    }

                    rapidjson::Value user_obj(rapidjson::kObjectType);

                    user_obj.AddMember("username", rapidjson::Value(username.c_str(), allocator), allocator);
                    user_obj.AddMember("role", rapidjson::Value(UserRole::RoleTypeToString(role).c_str(), allocator), allocator);

                    users_array.PushBack(user_obj, allocator);

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[USER_API] Added user %s to response", username.c_str());
                }

                doc.AddMember("users", users_array, allocator);

                doc.AddMember("debug_info", rapidjson::Value("Response generated at " + std::to_string(time(nullptr)), allocator), allocator);

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                doc.Accept(writer);

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[USER_API] Sending response with %d users", users_array.Size());

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[USER_API] Response JSON: %s", buffer.GetString());

                cb(buffer.GetString());
            }
            else if (method == "POST") {
                std::string body = ctx->body().ToString();

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[USER_API] Received POST request with body: %s", body.c_str());

                rapidjson::Document doc;
                rapidjson::ParseResult parseResult = doc.Parse(body.c_str());

                if (parseResult.IsError() || !doc.IsObject() ||
                    !doc.HasMember("username") || !doc["username"].IsString() ||
                    !doc.HasMember("role") || !doc["role"].IsString()) {

                    ctx->set_response_http_code(400);
                    cb("{\"success\": false, \"message\": \"Invalid request format\"}");
                    return;
                }

                std::string username = doc["username"].GetString();
                std::string role_str = doc["role"].GetString();
                std::string password;
                std::string action = "update";   
                std::string originalUsername = username;

                if (doc.HasMember("action") && doc["action"].IsString()) {
                    action = doc["action"].GetString();
                }

                if (doc.HasMember("originalUsername") && doc["originalUsername"].IsString()) {
                    originalUsername = doc["originalUsername"].GetString();
                }

                if (doc.HasMember("password") && doc["password"].IsString()) {
                    password = doc["password"].GetString();
                }

                RoleType role = UserRole::StringToRoleType(role_str);

                std::string current_username = get_username_from_session(ctx);
                bool is_admin_user = is_admin(ctx);

                if (!is_admin_user) {
                    if (originalUsername != current_username) {
                        ctx->set_response_http_code(403);
                        cb("{\"success\": false, \"message\": \"You can only update your own account\"}");
                        return;
                    }

                    RoleType current_role = UserRole::GetRoleByUsername(originalUsername);
                    if (current_role == RoleType::UNKNOWN) {
                        ctx->set_response_http_code(404);
                        cb("{\"success\": false, \"message\": \"User not found\"}");
                        return;
                    }

                    role = current_role;
                }

                bool success = false;

                if (action == "rename") {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[USER_API] Renaming user from '%s' to '%s'",
                        originalUsername.c_str(), username.c_str());

                    if (username.empty()) {
                        ctx->set_response_http_code(400);
                        cb("{\"success\": false, \"message\": \"Username cannot be empty\"}");
                        return;
                    }

                    auto users = UserRole::GetAllUsers();
                    for (const auto& user_tuple : users) {
                        if (std::get<0>(user_tuple) == username && username != originalUsername) {
                            ctx->set_response_http_code(400);
                            cb("{\"success\": false, \"message\": \"Username already exists\"}");
                            return;
                        }
                    }

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[USER_API] Found %d users before rename operation", users.size());

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[USER_API] Calling RenameUser(%s, %s)",
                        originalUsername.c_str(), username.c_str());

                    success = UserRole::RenameUser(originalUsername, username);

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[USER_API] RenameUser result: %s", success ? "success" : "failure");

                    auto users_after = UserRole::GetAllUsers();
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[USER_API] Found %d users after rename operation", users_after.size());

                    bool old_user_exists = false;
                    bool new_user_exists = false;

                    for (const auto& user_tuple : users_after) {
                        std::string current_username = std::get<0>(user_tuple);
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[USER_API] User after rename: %s", current_username.c_str());

                        if (current_username == originalUsername) {
                            old_user_exists = true;
                        }
                        if (current_username == username) {
                            new_user_exists = true;
                        }
                    }

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[USER_API] After rename: old_user_exists=%s, new_user_exists=%s",
                        old_user_exists ? "true" : "false",
                        new_user_exists ? "true" : "false");

                    if (success && old_user_exists) {
                        ctx->set_response_http_code(500);
                        cb("{\"success\": false, \"message\": \"Failed to delete original user\"}");
                        return;
                    }

                    if (success && !new_user_exists) {
                        ctx->set_response_http_code(500);
                        cb("{\"success\": false, \"message\": \"Failed to create new user\"}");
                        return;
                    }

                    if (success) {
                        cb("{\"success\": true, \"message\": \"User renamed successfully\"}");
                    }
                    else {
                        ctx->set_response_http_code(500);
                        cb("{\"success\": false, \"message\": \"Failed to rename user\"}");
                    }
                    return;
                }
                else {
                    std::string body = ctx->body().ToString();

                    rapidjson::Document doc;
                    doc.Parse(body.c_str());

                    if (doc.HasParseError() || !doc.IsObject() ||
                        !doc.HasMember("username") || !doc["username"].IsString() ||
                        !doc.HasMember("role") || !doc["role"].IsString()) {

                        ctx->set_response_http_code(400);
                        cb("{\"success\": false, \"message\": \"Invalid request format\"}");
                        return;
                    }

                    std::string username = doc["username"].GetString();
                    std::string role_str = doc["role"].GetString();
                    std::string password;

                    if (doc.HasMember("password") && doc["password"].IsString()) {
                        password = doc["password"].GetString();
                    }

                    RoleType role = UserRole::StringToRoleType(role_str);

                    std::string current_username = get_username_from_session(ctx);
                    bool is_admin_user = is_admin(ctx);

                    if (!is_admin_user) {
                        if (username != current_username) {
                            ctx->set_response_http_code(403);
                            cb("{\"success\": false, \"message\": \"You can only update your own account\"}");
                            return;
                        }

                        RoleType current_role = UserRole::GetRoleByUsername(username);
                        if (current_role == RoleType::UNKNOWN) {
                            ctx->set_response_http_code(404);
                            cb("{\"success\": false, \"message\": \"User not found\"}");
                            return;
                        }

                        role = current_role;
                    }

                    bool success;
                    if (password.empty()) {
                        auto users = UserRole::GetAllUsers();
                        bool user_exists = false;

                        for (const auto& user_tuple : users) {
                            if (std::get<0>(user_tuple) == username) {
                                user_exists = true;
                                break;
                            }
                        }

                        if (!user_exists) {
                            ctx->set_response_http_code(404);
                            cb("{\"success\": false, \"message\": \"User not found\"}");
                            return;
                        }

                        std::string current_password;
                        success = UserRole::CreateOrUpdateUser(username, password, role);
                    }
                    else {
                        success = UserRole::CreateOrUpdateUser(username, password, role);
                    }

                    if (success) {
                        cb("{\"success\": true, \"message\": \"User saved successfully\"}");
                    }
                    else {
                        ctx->set_response_http_code(500);
                        cb("{\"success\": false, \"message\": \"Failed to save user\"}");
                    }
                }
            }
            else if (method == "DELETE") {
                std::string body = ctx->body().ToString();

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[USER_API] Received DELETE request with body: '%s'", body.c_str());

                std::string username;

                if (!body.empty()) {
                    rapidjson::Document doc;
                    rapidjson::ParseResult parseResult = doc.Parse(body.c_str());
                    if (parseResult.IsError()) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[USER_API] JSON parse error at offset %zu",
                            doc.GetErrorOffset());
                    }
                    else if (!doc.IsObject()) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[USER_API] JSON is not an object");
                    }
                    else if (!doc.HasMember("username")) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[USER_API] Missing 'username' field in request");
                    }
                    else if (!doc["username"].IsString()) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[USER_API] 'username' field is not a string");
                    }
                    else {
                        username = doc["username"].GetString();
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[USER_API] Got username from JSON body: %s", username.c_str());
                    }
                }

                if (username.empty()) {
                    std::string uri = ctx->original_uri();
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[USER_API] Checking URI for username: %s", uri.c_str());

                    size_t pos = uri.find("username=");
                    if (pos != std::string::npos) {
                        std::string param = uri.substr(pos + 9);    

                        size_t end_pos = param.find('&');
                        if (end_pos != std::string::npos) {
                            param = param.substr(0, end_pos);
                        }

                        username = param;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[USER_API] Got username from query parameter: %s", username.c_str());
                    }
                }

                if (username.empty()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[USER_API] No username provided in request");
                    ctx->set_response_http_code(400);
                    cb("{\"success\": false, \"message\": \"Username is required\"}");
                    return;
                }

                if (username == "admin") {
                    ctx->set_response_http_code(400);
                    cb("{\"success\": false, \"message\": \"Cannot delete admin user\"}");
                    return;
                }

                std::string current_username = get_username_from_session(ctx);
                bool is_admin_user = is_admin(ctx);

                if (!is_admin_user && username != current_username) {
                    ctx->set_response_http_code(403);
                    cb("{\"success\": false, \"message\": \"You can only delete your own account\"}");
                    return;
                }

                bool success = UserRole::DeleteUser(username);

                if (success) {
                    cb("{\"success\": true, \"message\": \"User deleted successfully\"}");
                }
                else {
                    ctx->set_response_http_code(500);
                    cb("{\"success\": false, \"message\": \"Failed to delete user\"}");
                }
            }
            else {
                ctx->set_response_http_code(405);
                cb("{\"success\": false, \"message\": \"Method not allowed\"}");
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                "Users API error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"success\": false, \"message\": \"Internal server error\"}");
        }
    }

    void Dashboard::handle_town_operations(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[TOWN OPS] Received request: RemoteIP: '%s', URI: '%s'",
                std::string(ctx->remote_ip()).c_str(),
                std::string(ctx->uri()).c_str());

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (doc.HasParseError()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN OPS] Invalid JSON in request body");
                headers::set_json_response(ctx);
                ctx->set_response_http_code(400);
                cb("{\"error\":\"Invalid JSON\"}");
                return;
            }

            auto& session = tsto::Session::get();

            if (doc.HasMember("operation") && doc["operation"].IsString()) {
                std::string operation = doc["operation"].GetString();

                if (operation == "load") {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Processing load operation");

                    if (!doc.HasMember("email") || !doc["email"].IsString()) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Missing email in load request");
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(400);
                        cb("{\"error\":\"Missing email parameter\"}");
                        return;
                    }

                    std::string email = doc["email"].GetString();
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Loading town for email: %s", email.c_str());

                    std::string town_file = "towns/" + email + ".pb";

                    if (!std::filesystem::exists(town_file)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Town file does not exist: %s", town_file.c_str());
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(404);
                        cb("{\"error\":\"Town not found for the specified email\"}");
                        return;
                    }

                    headers::set_json_response(ctx);
                    cb("{\"success\":true,\"message\":\"Town found\",\"filePath\":\"" + town_file + "\"}");
                    return;
                }
                else if (operation == "save") {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Processing save operation");

                    if (!doc.HasMember("email") || !doc["email"].IsString()) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Missing email in save request");
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(400);
                        cb("{\"error\":\"Missing email parameter\"}");
                        return;
                    }

                    std::string email = doc["email"].GetString();
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Saving town as email: %s", email.c_str());

                    std::string current_town = session.town_filename;
                    if (current_town.empty()) {
                        current_town = "mytown.pb";
                    }

                    std::string source_path = "towns/" + current_town;
                    std::string target_path = "towns/" + email + ".pb";

                    try {
                        if (!std::filesystem::exists("towns")) {
                            std::filesystem::create_directory("towns");
                        }

                        std::filesystem::copy_file(
                            source_path,
                            target_path,
                            std::filesystem::copy_options::overwrite_existing
                        );

                        tsto::land::Land::create_default_currency_file(email);

                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Town saved successfully as: %s", target_path.c_str());

                        headers::set_json_response(ctx);
                        cb("{\"success\":true,\"message\":\"Town saved successfully\"}");
                    }
                    catch (const std::exception& ex) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Failed to save town: %s", ex.what());
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(500);
                        cb("{\"error\":\"Failed to save town: " + std::string(ex.what()) + "\"}");
                    }
                    return;
                }
                else if (operation == "copy") {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Processing copy operation");

                    if (!doc.HasMember("sourceEmail") || !doc["sourceEmail"].IsString() ||
                        !doc.HasMember("targetEmail") || !doc["targetEmail"].IsString()) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Missing sourceEmail or targetEmail in copy request");
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(400);
                        cb("{\"error\":\"Missing sourceEmail or targetEmail parameters\"}");
                        return;
                    }

                    std::string sourceEmail = doc["sourceEmail"].GetString();
                    std::string targetEmail = doc["targetEmail"].GetString();

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Copying town from %s to %s", sourceEmail.c_str(), targetEmail.c_str());

                    std::string source_path = "towns/" + sourceEmail + ".pb";
                    std::string target_path = "towns/" + targetEmail + ".pb";

                    if (!std::filesystem::exists(source_path)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Source town does not exist: %s", source_path.c_str());
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(404);
                        cb("{\"error\":\"Source town not found\"}");
                        return;
                    }

                    try {
                        if (!std::filesystem::exists("towns")) {
                            std::filesystem::create_directory("towns");
                        }

                        std::filesystem::copy_file(
                            source_path,
                            target_path,
                            std::filesystem::copy_options::overwrite_existing
                        );

                        tsto::land::Land::create_default_currency_file(targetEmail);

                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Town copied successfully from %s to %s", source_path.c_str(), target_path.c_str());

                        headers::set_json_response(ctx);
                        cb("{\"success\":true,\"message\":\"Town copied successfully\"}");
                    }
                    catch (const std::exception& ex) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Failed to copy town: %s", ex.what());
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(500);
                        cb("{\"error\":\"Failed to copy town: " + std::string(ex.what()) + "\"}");
                    }
                    return;
                }
                else if (operation == "import") {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Processing import operation");

                    std::string email;
                    if (doc.HasMember("email") && doc["email"].IsString()) {
                        email = doc["email"].GetString();
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Email provided: %s", email.c_str());
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] No email provided, using default filename");
                    }

                    if (!doc.HasMember("filePath") || !doc["filePath"].IsString()) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Missing filePath in import request");
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(400);
                        cb("{\"error\":\"Missing filePath\"}");
                        return;
                    }

                    std::string temp_file_path = doc["filePath"].GetString();
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Temp file path: %s", temp_file_path.c_str());

                    std::string filename = temp_file_path.substr(temp_file_path.find_last_of("/\\") + 1);
                    std::string extension;
                    size_t dot_pos = filename.find_last_of(".");

                    if (dot_pos != std::string::npos) {
                        extension = filename.substr(dot_pos + 1);
                    }

                    if (extension != "pb") {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Invalid file format: %s (only .pb files are allowed)", extension.c_str());
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(400);
                        cb("{\"error\":\"Invalid file format. Only .pb files are allowed.\"}");
                        return;
                    }

                    try {
                        std::ifstream file(temp_file_path, std::ios::binary);
                        if (!file.is_open()) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                                "[TOWN OPS] Failed to open file for validation: %s", temp_file_path.c_str());
                            headers::set_json_response(ctx);
                            ctx->set_response_http_code(500);
                            cb("{\"error\":\"Failed to open file for validation\"}");
                            return;
                        }

                        file.seekg(0, std::ios::end);
                        size_t file_size = file.tellg();
                        file.seekg(0, std::ios::beg);

                        if (file_size < 10) {         
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                                "[TOWN OPS] File too small to be a valid town file: %zu bytes", file_size);
                            headers::set_json_response(ctx);
                            ctx->set_response_http_code(400);
                            cb("{\"error\":\"Invalid town file - file too small\"}");
                            return;
                        }

                        char buffer[256];
                        file.read(buffer, std::min(file_size, size_t(256)));
                        size_t read_bytes = file.gcount();

                        bool is_text_file = true;
                        for (size_t i = 0; i < read_bytes; i++) {
                            if (buffer[i] < 32 && buffer[i] != 9 && buffer[i] != 10 && buffer[i] != 13) {
                                is_text_file = false;
                                break;
                            }
                        }

                        if (is_text_file) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                                "[TOWN OPS] File appears to be a text file, not a valid protobuf");
                            headers::set_json_response(ctx);
                            ctx->set_response_http_code(400);
                            cb("{\"error\":\"Invalid town file - appears to be a text file, not a protobuf\"}");
                            return;
                        }
                    }
                    catch (const std::exception& ex) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Error validating file: %s", ex.what());
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(500);
                        cb("{\"error\":\"Error validating file: " + std::string(ex.what()) + "\"}");
                        return;
                    }

                    if (!std::filesystem::exists("towns")) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Creating towns directory");
                        std::filesystem::create_directory("towns");
                    }

                    std::string target_file;
                    std::string currency_email;

                    if (email.empty()) {
                        target_file = "towns/mytown.pb";
                        currency_email = "default";
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] No email provided, using default filename in towns directory: %s", target_file.c_str());
                    }
                    else {
                        target_file = "towns/" + email + ".pb";
                        currency_email = email;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Using email-based filename: %s", target_file.c_str());
                    }

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Target file path: %s", target_file.c_str());

                    try {
                        if (!std::filesystem::exists(temp_file_path)) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                                "[TOWN OPS] Source file does not exist: %s", temp_file_path.c_str());
                            headers::set_json_response(ctx);
                            ctx->set_response_http_code(500);
                            cb("{\"error\":\"Source file not found\"}");
                            return;
                        }

                        std::filesystem::path cwd = std::filesystem::current_path();
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Current working directory: %s", cwd.string().c_str());

                        std::filesystem::path target_path(target_file);
                        std::filesystem::path parent_path = target_path.parent_path();
                        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                                "[TOWN OPS] Creating parent directory: %s", parent_path.string().c_str());
                            std::filesystem::create_directories(parent_path);
                        }

                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Importing town file from %s for email %s",
                            temp_file_path.c_str(),
                            currency_email.empty() ? "<default>" : currency_email.c_str());

                        if (tsto::land::Land::import_town_file(temp_file_path, currency_email)) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                                "[TOWN OPS] Town file imported successfully using Land::import_town_file");

                        }
                        else {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                                "[TOWN OPS] Failed to import town file using Land::import_town_file");
                            headers::set_json_response(ctx);
                            ctx->set_response_http_code(500);
                            cb("{\"error\":\"Failed to import town file\"}");
                            return;
                        }

                        std::filesystem::remove(temp_file_path);

                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Town file imported successfully: %s", target_file.c_str());

                        std::string webpanel_username = get_username_from_session(ctx);
                        if (webpanel_username.empty()) {
                            webpanel_username = "unknown_admin";        
                        }

                        if (UserRole::RecordTownUpload(webpanel_username)) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                                "[STATISTICS] Recorded town upload for webpanel user: %s", webpanel_username.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                                "[STATISTICS] Failed to record town upload for webpanel user: %s", webpanel_username.c_str());
                        }

                        headers::set_json_response(ctx);
                        cb("{\"success\":true,\"message\":\"Town imported successfully\"}");
                    }
                    catch (const std::exception& ex) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Failed to import town file: %s", ex.what());
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(500);
                        cb("{\"error\":\"Failed to import town file: " + std::string(ex.what()) + "\"}");
                    }
                    return;
                }
                else {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Unknown operation in request: %s", operation.c_str());
                    headers::set_json_response(ctx);
                    ctx->set_response_http_code(400);
                    cb("{\"error\":\"Unknown operation\"}");
                    return;
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN OPS] Missing operation in request");
                headers::set_json_response(ctx);
                ctx->set_response_http_code(400);
                cb("{\"error\":\"Missing operation\"}");
                return;
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[TOWN OPS] Exception: %s", ex.what());
            headers::set_json_response(ctx);
            ctx->set_response_http_code(500);
            cb("{\"error\":\"Internal server error\"}");
        }
    }

    void Dashboard::handle_security_operations(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            const std::string method = ctx->GetMethod();

            if (method == "GET") {
                auto& security = tsto::security::LoginSecurity::get_instance();
                std::vector<std::string> locked_accounts = security.get_locked_accounts();

                rapidjson::Document response;
                response.SetObject();
                auto& allocator = response.GetAllocator();

                response.AddMember("success", true, response.GetAllocator());

                rapidjson::Value accounts_array(rapidjson::kArrayType);

                for (const auto& username : locked_accounts) {
                    accounts_array.PushBack(
                        rapidjson::Value(username.c_str(), allocator).Move(),
                        allocator
                    );
                }

                response.AddMember("locked_accounts", accounts_array, allocator);

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(200);
                cb(buffer.GetString());
            }
            else if (method == "POST") {
                std::string body = ctx->body().ToString();
                rapidjson::Document doc;
                doc.Parse(body.c_str());

                if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("username") || !doc["username"].IsString()) {
                    rapidjson::Document response;
                    response.SetObject();
                    response.AddMember("success", false, response.GetAllocator());
                    response.AddMember("error", "Invalid request format", response.GetAllocator());

                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    response.Accept(writer);

                    ctx->AddResponseHeader("Content-Type", "application/json");
                    ctx->set_response_http_code(400);
                    cb(buffer.GetString());
                    return;
                }

                std::string username = doc["username"].GetString();

                auto& security = tsto::security::LoginSecurity::get_instance();
                bool success = security.unlock_account(username);

                rapidjson::Document response;
                response.SetObject();
                auto& allocator = response.GetAllocator();

                response.AddMember("success", success, allocator);

                if (success) {
                    response.AddMember("message",
                        rapidjson::Value(("Account '" + username + "' has been unlocked successfully").c_str(), allocator),
                        allocator);

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SECURITY,
                        "[SECURITY] Admin unlocked account: %s", username.c_str());
                }
                else {
                    response.AddMember("error",
                        rapidjson::Value(("Account '" + username + "' was not locked or does not exist").c_str(), allocator),
                        allocator);
                }

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(success ? 200 : 400);
                cb(buffer.GetString());
            }
            else {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "Method not allowed", response.GetAllocator());

                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);

                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(405);
                cb(buffer.GetString());
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SECURITY,
                "[SECURITY] Error handling security operations: %s", ex.what());

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Internal server error", response.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->set_response_http_code(500);
            cb(buffer.GetString());
        }
    }

    void Dashboard::handle_export_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            if (!has_town_operations_access(ctx)) {
                redirect_to_access_denied(ctx, cb);
                return;
            }

            std::string encoded_email = ctx->GetQuery("email");
            if (encoded_email.empty()) {
                std::string error_msg = R"({"error": "Missing email parameter"})";
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, error_msg.c_str());
                headers::set_json_response(ctx);
                ctx->set_response_http_code(400);
                cb(error_msg);
                return;
            }

            std::string email;
            for (size_t i = 0; i < encoded_email.length(); ++i) {
                if (encoded_email[i] == '%' && i + 2 < encoded_email.length()) {
                    int value;
                    std::istringstream iss(encoded_email.substr(i + 1, 2));
                    iss >> std::hex >> value;
                    email += static_cast<char>(value);
                    i += 2;
                }
                else if (encoded_email[i] == '+') {
                    email += ' ';
                }
                else {
                    email += encoded_email[i];
                }
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
                "Decoded email from '%s' to '%s'", encoded_email.c_str(), email.c_str());

            auto& db = tsto::database::Database::get_instance();
            std::string townFilePath;
            std::string userSaveFile;

            bool found_town_file = false;
            if (db.get_land_save_path(email, townFilePath) && !townFilePath.empty()) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Found town file path in database for %s: %s", email.c_str(), townFilePath.c_str());

                size_t lastSlash = townFilePath.find_last_of("/\\");
                if (lastSlash != std::string::npos) {
                    userSaveFile = townFilePath.substr(lastSlash + 1);
                }
                else {
                    userSaveFile = townFilePath;
                }

                if (std::filesystem::exists(townFilePath)) {
                    found_town_file = true;
                }
                else {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                        "Town file from database not found at %s, will try alternatives", townFilePath.c_str());
                }
            }

            if (!found_town_file) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Scanning towns directory for %s's town file", email.c_str());

                std::filesystem::create_directories("towns");

                for (const auto& entry : std::filesystem::directory_iterator("towns")) {
                    if (entry.is_regular_file()) {
                        std::string filename = entry.path().filename().string();
                        if (filename == email + ".pb") {
                            townFilePath = entry.path().string();
                            userSaveFile = filename;
                            found_town_file = true;
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                                "Found town file: %s", townFilePath.c_str());
                            break;
                        }
                    }
                }

            }

            if (!found_town_file) {
                std::string error_msg = R"({"error": "Town file not found"})";
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, error_msg.c_str());
                headers::set_json_response(ctx);
                ctx->set_response_http_code(404);
                cb(error_msg);
                return;
            }

            std::ifstream file(townFilePath, std::ios::binary);
            if (!file.is_open()) {
                std::string error_msg = R"({"error": "Failed to open town file"})";
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, error_msg.c_str());
                headers::set_json_response(ctx);
                ctx->set_response_http_code(500);
                cb(error_msg);
                return;
            }

            std::string fileContent((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
            file.close();

            ctx->AddResponseHeader("Content-Type", "application/octet-stream");
            ctx->AddResponseHeader("Content-Disposition", "attachment; filename=\"" + userSaveFile + "\"");
            ctx->AddResponseHeader("Content-Length", std::to_string(fileContent.length()));

            cb(fileContent);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Successfully exported town for user: %s (file: %s)", email.c_str(), townFilePath.c_str());

        }
        catch (const std::exception& e) {
            std::string error_msg = R"({"error": "Error exporting town: ")" + std::string(e.what()) + R"("})";
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, error_msg.c_str());
            headers::set_json_response(ctx);
            ctx->set_response_http_code(500);
            cb(error_msg);
        }
    }

    void Dashboard::handle_statistics(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        bool include_upload_stats = is_admin(ctx);

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            int active_connections = tsto::statistics::Statistics::get_instance().get_active_connections();
            doc.AddMember("active_connections", active_connections, allocator);

            int unique_clients = tsto::statistics::Statistics::get_instance().get_unique_clients();
            doc.AddMember("unique_clients", unique_clients, allocator);
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[STATISTICS] Sending statistics: unique_clients=%d, active_connections=%d",
                unique_clients, active_connections);

            if (include_upload_stats) {
                auto upload_stats = UserRole::GetTownUploadStats();

                rapidjson::Value uploads_array(rapidjson::kArrayType);

                for (const auto& [username, total_uploads, last_upload] : upload_stats) {
                    rapidjson::Value user_obj(rapidjson::kObjectType);

                    user_obj.AddMember("username", rapidjson::Value(username.c_str(), allocator), allocator);

                    user_obj.AddMember("total_uploads", total_uploads, allocator);

                    user_obj.AddMember("last_upload", rapidjson::Value(last_upload.c_str(), allocator), allocator);

                    uploads_array.PushBack(user_obj, allocator);
                }

                doc.AddMember("upload_stats", uploads_array, allocator);
            }

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[STATISTICS] Error generating statistics: %s", ex.what());
            cb("{\"error\": \"Failed to generate statistics\"}");
        }
    }

    void Dashboard::handle_clear_town_upload_stats(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            bool success = UserRole::ClearTownUploadStats();

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            doc.AddMember("success", success, allocator);

            if (success) {
                doc.AddMember("message", rapidjson::Value("Town upload statistics cleared successfully", allocator), allocator);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[STATISTICS] Town upload statistics cleared by admin: %s", get_username_from_session(ctx).c_str());
            }
            else {
                doc.AddMember("error", rapidjson::Value("Failed to clear town upload statistics", allocator), allocator);
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[STATISTICS] Failed to clear town upload statistics by admin: %s", get_username_from_session(ctx).c_str());
            }

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[STATISTICS] Error clearing town upload statistics: %s", ex.what());
            cb("{\"success\": false, \"error\": \"Failed to clear town upload statistics\"}");
        }
    }

    void Dashboard::handle_delete_town(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!is_authenticated(ctx)) {
            redirect_to_login(ctx, cb);
            return;
        }

        if (!is_admin(ctx) && !has_town_operations_access(ctx)) {
            redirect_to_access_denied(ctx, cb);
            return;
        }

        ctx->AddResponseHeader("Content-Type", "application/json");

        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc["email"].IsString()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[DELETE TOWN] Invalid request format");
                cb("{\"success\": false, \"error\": \"Invalid request format\"}");
                return;
            }

            std::string email = doc["email"].GetString();
            if (email.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[DELETE TOWN] Empty email provided");
                cb("{\"success\": false, \"error\": \"Email cannot be empty\"}");
                return;
            }

            auto& db = tsto::database::Database::get_instance();

            std::string land_save_path;
            if (!db.get_land_save_path(email, land_save_path) || land_save_path.empty()) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[DELETE TOWN] No land save path found in database for email: %s, checking default path", email.c_str());

                std::vector<std::string> possible_paths = {
                    "towns/" + email + ".pb",
                    "towns/anon_" + email + ".pb",
                    "towns/mytown.pb"    
                };

                bool found_path = false;
                for (const auto& path : possible_paths) {
                    if (std::filesystem::exists(path)) {
                        land_save_path = path;
                        found_path = true;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[DELETE TOWN] Found town file at default path: %s", land_save_path.c_str());
                        break;
                    }
                }

                if (!found_path) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[DELETE TOWN] No town file found for email: %s", email.c_str());
                    cb("{\"success\": false, \"error\": \"No town file found for this user\"}");
                    return;
                }
            }

            if (!std::filesystem::exists(land_save_path)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[DELETE TOWN] Town file does not exist: %s", land_save_path.c_str());
                cb("{\"success\": false, \"error\": \"Town file does not exist\"}");
                return;
            }

            std::string backup_dir = "towns/backups/";
            std::filesystem::create_directories(backup_dir);

            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::stringstream ss;
            ss << backup_dir << "deleted_" << email << "_" << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S") << ".pb";
            std::string backup_path = ss.str();

            try {
                std::filesystem::copy_file(land_save_path, backup_path, std::filesystem::copy_options::overwrite_existing);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[DELETE TOWN] Created backup at: %s", backup_path.c_str());
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[DELETE TOWN] Failed to create backup: %s", ex.what());
            }

            bool deleted = false;
            try {
                deleted = std::filesystem::remove(land_save_path);
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[DELETE TOWN] Exception while deleting town file: %s", ex.what());
                cb("{\"success\": false, \"error\": \"Failed to delete town file\"}");
                return;
            }

            if (!deleted) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[DELETE TOWN] Failed to delete town file: %s", land_save_path.c_str());
                cb("{\"success\": false, \"error\": \"Failed to delete town file\"}");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[DELETE TOWN] Successfully deleted town file for %s at %s",
                email.c_str(), land_save_path.c_str());

            cb("{\"success\": true, \"message\": \"Town deleted successfully\"}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[DELETE TOWN] Error deleting town: %s", ex.what());
            cb("{\"success\": false, \"error\": \"Internal server error\"}");
        }
    }
}
