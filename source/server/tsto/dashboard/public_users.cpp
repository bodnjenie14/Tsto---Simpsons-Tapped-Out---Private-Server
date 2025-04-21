#include <std_include.hpp>
#include "public_users.hpp"
#include "dashboard.hpp"
#include "../database/public_users_db.hpp"
#include "../database/database.hpp"       
#include "../land/new_land.hpp"
#include "debugging/serverlog.hpp"
#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <regex>
#include <chrono>
#include <random>
#include "tsto/auth/login.hpp"

namespace tsto::dashboard {

    void handle_public_login(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_public_validate_token(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_public_town_info(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_public_import_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_public_login_page(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_public_request_code(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_register_public_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_public_update_display_name(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);

    void handle_admin_import_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_admin_export_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_admin_delete_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_admin_save_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);

    std::string url_decode(const std::string& encoded) {
        std::string decoded;
        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                int value;
                std::istringstream is(encoded.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    decoded += static_cast<char>(value);
                    i += 2;
                }
                else {
                    decoded += encoded[i];
                }
            }
            else if (encoded[i] == '+') {
                decoded += ' ';
            }
            else {
                decoded += encoded[i];
            }
        }
        return decoded;
    }

    std::string generate_token(size_t length) {
        static const char alphanum[] =
            "0123456789"
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
            "abcdefghijklmnopqrstuvwxyz";

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, sizeof(alphanum) - 2);

        std::string token;
        token.reserve(length);

        for (size_t i = 0; i < length; ++i) {
            token += alphanum[dis(gen)];
        }

        return token;
    }

    namespace {
        void set_json_response(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->AddResponseHeader("Server", "TSTO-Server/1.0");
            ctx->AddResponseHeader("Cache-Control", "no-store, no-cache, must-revalidate");
            ctx->AddResponseHeader("Access-Control-Allow-Origin", "*");
            ctx->AddResponseHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            ctx->AddResponseHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        }

        bool is_valid_email(const std::string& email) {
            const std::regex email_regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
            return std::regex_match(email, email_regex);
        }

        int64_t get_current_timestamp_ms() {
            return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
        }
    }

    void handle_public_request_code(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc["email"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string email = doc["email"].GetString();
            std::string display_name = "";

            if (doc.HasMember("display_name") && doc["display_name"].IsString()) {
                display_name = doc["display_name"].GetString();
            }

            auto& db = tsto::database::PublicUsersDB::get_instance();
            if (!db.user_exists(email)) {
                ctx->set_response_http_code(404);
                cb(R"({"error": "User not found"})");
                return;
            }

            bool api_enabled = utils::configuration::ReadBoolean("TSTO_API", "Enabled", true);    
            bool smtp_enabled = utils::configuration::ReadBoolean("SMTP", "Enabled", false);

            std::string verification_code;

            if (api_enabled) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Using TSTO API to generate verification code for user: %s", email.c_str());

                tsto::login::Login login;
                verification_code = login.handle_tsto_api(email, display_name);

                if (verification_code.empty()) {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                        "TSTO API call failed, using default verification code for user: %s", email.c_str());
                    verification_code = "123456";
                }
            }
            else {
                verification_code = db.generate_verification_code(email);

                if (verification_code.empty()) {
                    ctx->set_response_http_code(500);
                    cb(R"({"error": "Failed to generate verification code"})");
                    return;
                }

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Generated verification code for user: %s, code: %s", email.c_str(), verification_code.c_str());
            }

            if (!db.update_user_cred(email, verification_code)) {
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to store verification code"})");
                return;
            }

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Verification code sent"})");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error handling verification code request: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_login(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc["email"].IsString() ||
                !doc.HasMember("password") || !doc["password"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string email = doc["email"].GetString();
            std::string password = doc["password"].GetString();

            if (!is_valid_email(email)) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid email format"})");
                return;
            }

            auto& db = tsto::database::PublicUsersDB::get_instance();
            bool auth_success = db.authenticate_user(email, password);

            if (!auth_success) {
                ctx->set_response_http_code(401);
                cb(R"({"success": false, "error": "Invalid email or password"})");
                return;
            }

            std::string token = generate_token(32);

            int64_t current_time = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            int64_t expiry = current_time + (24 * 60 * 60);   

            bool token_stored = db.store_access_token(email, token, expiry);

            if (!token_stored) {
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to store access token"})");
                return;
            }

            std::string display_name;
            bool is_verified;
            int64_t registration_date;
            int64_t last_login;
            db.get_user_info(email, display_name, is_verified, registration_date, last_login);

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", true, response.GetAllocator());
            response.AddMember("token", rapidjson::Value(token.c_str(), response.GetAllocator()), response.GetAllocator());

            rapidjson::Value user_info(rapidjson::kObjectType);
            user_info.AddMember("email", rapidjson::Value(email.c_str(), response.GetAllocator()), response.GetAllocator());
            user_info.AddMember("display_name", rapidjson::Value(display_name.c_str(), response.GetAllocator()), response.GetAllocator());
            user_info.AddMember("is_verified", is_verified, response.GetAllocator());

            response.AddMember("user_info", user_info, response.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->set_response_http_code(200);
            cb(buffer.GetString());

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Public user logged in: %s", email.c_str());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error handling public login: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_register_public_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc["email"].IsString() ||
                !doc.HasMember("password") || !doc["password"].IsString() ||
                !doc.HasMember("display_name") || !doc["display_name"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string email = doc["email"].GetString();
            std::string password = doc["password"].GetString();
            std::string display_name = doc["display_name"].GetString();

            if (!is_valid_email(email)) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid email format"})");
                return;
            }

            if (password.length() < 8 ||
                !std::regex_search(password, std::regex("[A-Z]")) ||
                !std::regex_search(password, std::regex("[a-z]")) ||
                !std::regex_search(password, std::regex("[0-9]"))) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Password does not meet requirements"})");
                return;
            }

            if (display_name.empty() || !std::regex_match(display_name, std::regex("^[a-zA-Z0-9 ]+$"))) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid display name"})");
                return;
            }

            auto& db = tsto::database::PublicUsersDB::get_instance();
            bool registered = db.register_user(email, password, display_name);

            if (!registered) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Registration failed. Email may already be in use."})");
                return;
            }

            bool api_enabled = utils::configuration::ReadBoolean("TSTO_API", "Enabled", false);
            bool smtp_enabled = utils::configuration::ReadBoolean("SMTP", "Enabled", false);

            std::string default_code = utils::configuration::ReadString("Auth", "DefaultVerificationCode", "123456");

            std::string verification_code;

            if (!api_enabled && !smtp_enabled) {
                verification_code = default_code;
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "Using default verification code (%s) for user: %s since both TSTO API and SMTP are disabled",
                    default_code.c_str(), email.c_str());
            }
            else {
                tsto::login::Login login;
                verification_code = login.handle_tsto_api(email, display_name);

                if (verification_code.empty()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                        "Failed to generate verification code through TSTO API for user: %s", email.c_str());
                    verification_code = default_code;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                        "Using default verification code (%s) as fallback for user: %s",
                        default_code.c_str(), email.c_str());
                }
            }

            if (!db.update_user_cred(email, verification_code)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to store verification code for user: %s", email.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Registered new public user: %s with verification code: %s", email.c_str(), verification_code.c_str());
            }

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Registration successful. Please check your email for verification."})");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error handling public registration: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_town_info(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string email = url_decode(ctx->GetQuery("email"));

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
                "Decoded email: %s", email.c_str());

            if (email.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Email parameter is required"})");
                return;
            }

            std::string auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
                ctx->set_response_http_code(401);
                cb(R"({"error": "Authentication required"})");
                return;
            }

            std::string token = auth_header.substr(7);     

            std::string land_save_path;
            bool has_town = false;

            {
                std::string towns_dir = "towns";
                std::string expected_filename = email + ".pb";

                std::filesystem::create_directories(towns_dir);

                for (const auto& entry : std::filesystem::directory_iterator(towns_dir)) {
                    if (entry.is_regular_file()) {
                        std::string filename = entry.path().filename().string();
                        if (filename == expected_filename) {
                            land_save_path = entry.path().string();
                            has_town = true;

                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
                                "Found town file for %s at %s", email.c_str(), land_save_path.c_str());
                            break;
                        }
                    }
                }

                if (!has_town) {
                }
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();

            response.AddMember("hasTown", has_town, allocator);

            if (has_town) {
                std::error_code ec;
                auto file_size = std::filesystem::file_size(land_save_path, ec);
                auto last_modified = std::filesystem::last_write_time(land_save_path, ec);

                auto last_modified_time = std::filesystem::last_write_time(land_save_path);
                int64_t last_modified_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::filesystem::file_time_type::clock::now().time_since_epoch()).count();

                response.AddMember("size", static_cast<uint64_t>(file_size), allocator);
                response.AddMember("lastModified", last_modified_ms, allocator);
                response.AddMember("path", rapidjson::Value(land_save_path.c_str(), allocator), allocator);
            }

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->set_response_http_code(200);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error handling town info request: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_import_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->AddResponseHeader("Server", "TSTO-Server/1.0");
            ctx->AddResponseHeader("Cache-Control", "no-store, no-cache, must-revalidate");
            ctx->AddResponseHeader("Access-Control-Allow-Origin", "*");

            const size_t MAX_FILE_SIZE = 5 * 1024 * 1024;    
            const char* file_size_header = ctx->FindRequestHeader("X-File-Size");

            if (file_size_header) {
                size_t reported_size = 0;
                try {
                    reported_size = std::stoull(file_size_header);

                    if (reported_size > MAX_FILE_SIZE) {
                        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                            "Town import rejected: Reported file size %zu bytes exceeds limit of %zu bytes",
                            reported_size, MAX_FILE_SIZE);
                        ctx->set_response_http_code(413);     
                        cb(R"({"error": "File too large. Maximum allowed size is 5MB."})");
                        return;
                    }
                }
                catch (const std::exception& ex) {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                        "Invalid X-File-Size header value: %s", file_size_header);
                }
            }

            std::string body = ctx->body().ToString();
            if (body.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "No file content provided"})");
                return;
            }

            if (body.size() > MAX_FILE_SIZE) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                    "Town import rejected: Actual file size %zu bytes exceeds limit of %zu bytes",
                    body.size(), MAX_FILE_SIZE);
                ctx->set_response_http_code(413);     
                cb(R"({"error": "File too large. Maximum allowed size is 5MB."})");
                return;
            }

            std::filesystem::create_directories("towns");

            const char* filename_header = ctx->FindRequestHeader("X-Filename");
            if (!filename_header) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Missing filename"})");
                return;
            }

            std::string dest_path = "towns/" + std::string(filename_header);

            std::ofstream file(dest_path, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Failed to create file: %s", dest_path.c_str());
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to create file"})");
                return;
            }

            file.write(body.data(), body.size());
            file.close();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Successfully imported town to %s", dest_path.c_str());

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Town imported successfully"})");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error importing town: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_export_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string email = url_decode(ctx->GetQuery("email"));

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
                "Decoded email: %s", email.c_str());

            if (email.empty()) {
                set_json_response(ctx);
                ctx->set_response_http_code(400);
                cb(R"({"error": "Email parameter is required"})");
                return;
            }

            std::string auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
                set_json_response(ctx);
                ctx->set_response_http_code(401);
                cb(R"({"error": "Authentication required"})");
                return;
            }

            std::string token = auth_header.substr(7);     

            auto& db = tsto::database::PublicUsersDB::get_instance();
            if (!db.validate_access_token(email, token)) {
                set_json_response(ctx);
                ctx->set_response_http_code(403);
                cb(R"({"error": "Invalid token or unauthorized access"})");
                return;
            }

            std::string towns_dir = "towns";
            std::filesystem::create_directories(towns_dir);    

            std::string town_file_path;
            bool found_town = false;

            std::string expected_filename = email + ".pb";

            for (const auto& entry : std::filesystem::directory_iterator(towns_dir)) {
                if (entry.is_regular_file()) {
                    std::string filename = entry.path().filename().string();
                    if (filename == expected_filename) {
                        town_file_path = entry.path().string();
                        found_town = true;
                        break;
                    }
                }
            }

            if (!found_town) {
                set_json_response(ctx);
                ctx->set_response_http_code(404);
                cb(R"({"error": "Town not found"})");
                return;
            }

            std::ifstream file(town_file_path, std::ios::binary);
            if (!file.is_open()) {
                set_json_response(ctx);
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to open town file"})");
                return;
            }

            std::string file_content((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());
            file.close();

            ctx->AddResponseHeader("Content-Type", "application/octet-stream");
            ctx->AddResponseHeader("Content-Disposition", "attachment; filename=\"" + expected_filename + "\"");
            ctx->AddResponseHeader("Content-Length", std::to_string(file_content.length()));
            ctx->set_response_http_code(200);
            cb(file_content);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error handling town export: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_delete_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            std::string email;
            if (doc.IsObject() && doc.HasMember("email") && doc["email"].IsString()) {
                email = doc["email"].GetString();
            }

            if (email.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Email parameter is required"})");
                return;
            }

            std::string auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
                ctx->set_response_http_code(401);
                cb(R"({"error": "Authentication required"})");
                return;
            }

            std::string token = auth_header.substr(7);     

            auto& db = tsto::database::PublicUsersDB::get_instance();
            if (!db.validate_access_token(email, token)) {
                ctx->set_response_http_code(403);
                cb(R"({"error": "Invalid token or unauthorized access"})");
                return;
            }

            std::string towns_dir = "towns";
            std::string expected_filename = email + ".pb";
            std::string town_file_path = towns_dir + "/" + expected_filename;

            if (!std::filesystem::exists(town_file_path)) {
                ctx->set_response_http_code(404);
                cb(R"({"error": "Town not found"})");
                return;
            }

            bool deleted = std::filesystem::remove(town_file_path);

            if (!deleted) {
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to delete town file"})");
                return;
            }

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Town deleted successfully"})");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error handling town deletion: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_currency_info(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string email = url_decode(ctx->GetQuery("email"));

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
                "Decoded email for currency info: %s", email.c_str());

            if (email.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Email parameter is required"})");
                return;
            }

            std::string auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
                ctx->set_response_http_code(401);
                cb(R"({"error": "Authentication required"})");
                return;
            }

            std::string token = auth_header.substr(7);     

            std::string currency_file_path = "towns/" + email + ".txt";
            bool has_currency = std::filesystem::exists(currency_file_path);
            int donuts = 0;

            if (has_currency) {
                std::ifstream file(currency_file_path);
                if (file.is_open()) {
                    std::string line;
                    if (std::getline(file, line)) {
                        try {
                            donuts = std::stoi(line);
                        }
                        catch (const std::exception& ex) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                                "Error parsing donuts value: %s", ex.what());
                            donuts = 0;
                        }
                    }
                    file.close();

                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
                        "Found currency file for %s at %s with %d donuts", email.c_str(), currency_file_path.c_str(), donuts);
                }
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();

            response.AddMember("hasCurrency", has_currency, allocator);

            if (has_currency) {
                response.AddMember("donuts", donuts, allocator);
            }

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->set_response_http_code(200);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error handling currency info: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_save_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc.HasMember("donuts") ||
                !doc["email"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string email = doc["email"].GetString();
            int donuts = 0;

            if (doc["donuts"].IsInt()) {
                donuts = doc["donuts"].GetInt();
            }
            else if (doc["donuts"].IsString()) {
                try {
                    donuts = std::stoi(doc["donuts"].GetString());
                }
                catch (const std::exception& ex) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                        "Error parsing donuts value: %s", ex.what());
                    donuts = 0;
                }
            }

            if (email.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Email is required"})");
                return;
            }

            std::string auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header.empty() || auth_header.substr(0, 7) != "Bearer ") {
                ctx->set_response_http_code(401);
                cb(R"({"error": "Authentication required"})");
                return;
            }

            std::string token = auth_header.substr(7);     

            std::filesystem::create_directories("towns");

            if (donuts <= 0) {
                donuts = std::stoi(utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));
            }

            const int MAX_DONUTS = 100000;
            if (donuts > MAX_DONUTS) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                    "Donut amount %d exceeds maximum limit of %d for user %s, capping at maximum",
                    donuts, MAX_DONUTS, email.c_str());
                donuts = MAX_DONUTS;
            }

            std::string currency_path = "towns/" + email + ".txt";
            std::ofstream file(currency_path);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Failed to create or open currency file: %s", currency_path.c_str());
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to create or open currency file"})");
                return;
            }

            file << donuts;
            file.close();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Successfully saved %d donuts for %s to %s", donuts, email.c_str(), currency_path.c_str());

            rapidjson::Document response;
            response.SetObject();
            auto& allocator = response.GetAllocator();

            response.AddMember("status", "success", allocator);
            response.AddMember("message", "Currency updated successfully", allocator);
            response.AddMember("donuts", donuts, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->set_response_http_code(200);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error saving currency: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_validate_token(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc["email"].IsString() ||
                !doc.HasMember("token") || !doc["token"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string email = doc["email"].GetString();
            std::string token = doc["token"].GetString();

            if (!is_valid_email(email)) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid email format"})");
                return;
            }

            auto& db = tsto::database::PublicUsersDB::get_instance();
            bool token_valid = db.validate_access_token(email, token);

            if (!token_valid) {
                ctx->set_response_http_code(401);
                cb(R"({"success": false, "error": "Invalid or expired token"})");
                return;
            }

            std::string display_name;
            bool is_verified;
            int64_t registration_date;
            int64_t last_login;
            bool got_info = db.get_user_info(email, display_name, is_verified, registration_date, last_login);

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", true, response.GetAllocator());

            if (got_info) {
                rapidjson::Value user_info(rapidjson::kObjectType);
                user_info.AddMember("email", rapidjson::Value(email.c_str(), response.GetAllocator()), response.GetAllocator());
                user_info.AddMember("display_name", rapidjson::Value(display_name.c_str(), response.GetAllocator()), response.GetAllocator());
                user_info.AddMember("is_verified", is_verified, response.GetAllocator());
                user_info.AddMember("registration_date", registration_date, response.GetAllocator());
                user_info.AddMember("last_login", last_login, response.GetAllocator());

                response.AddMember("user_info", user_info, response.GetAllocator());
            }

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->set_response_http_code(200);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error validating token: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_login_page(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            ctx->AddResponseHeader("Content-Type", "text/html");
            ctx->AddResponseHeader("Server", "TSTO-Server/1.0");
            ctx->AddResponseHeader("Cache-Control", "no-store, no-cache, must-revalidate");

            std::string file_path = "webpanel/public_login.html";
            std::ifstream file(file_path);

            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Failed to open public login page: %s", file_path.c_str());
                ctx->set_response_http_code(404);
                cb("<html><body><h1>404 Not Found</h1><p>The requested page could not be found.</p></body></html>");
                return;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string html_content = buffer.str();

            ctx->set_response_http_code(200);
            cb(html_content);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error serving public login page: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("<html><body><h1>500 Internal Server Error</h1><p>An error occurred while processing your request.</p></body></html>");
        }
    }

    void handle_public_verify_code(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc["email"].IsString() ||
                !doc.HasMember("code") || !doc["code"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string email = doc["email"].GetString();
            std::string code = doc["code"].GetString();

            if (!is_valid_email(email)) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid email format"})");
                return;
            }

            auto& db = tsto::database::PublicUsersDB::get_instance();
            bool verification_success = db.verify_email(email, code);

            if (!verification_success) {
                ctx->set_response_http_code(400);
                cb(R"({"success": false, "error": "Invalid or expired verification code"})");
                return;
            }

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Email verified successfully"})");

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Email verified for public user: %s", email.c_str());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error verifying email: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_reset_password(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() ||
                !doc.HasMember("email") || !doc["email"].IsString() ||
                !doc.HasMember("code") || !doc["code"].IsString() ||
                !doc.HasMember("password") || !doc["password"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string email = doc["email"].GetString();
            std::string code = doc["code"].GetString();
            std::string password = doc["password"].GetString();

            if (email.empty() || code.empty() || password.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Email, code, and password are required"})");
                return;
            }

            if (password.length() < 8) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Password must be at least 8 characters long"})");
                return;
            }

            auto& db = tsto::database::PublicUsersDB::get_instance();
            bool success = db.reset_password(email, code, password);

            if (!success) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid verification code or user not found"})");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Password reset successful for user: %s", email.c_str());

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Password reset successful"})");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error handling password reset: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_get_display_name(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            const char* auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header == nullptr || std::string(auth_header).substr(0, 7) != "Bearer ") {
                ctx->set_response_http_code(401);
                cb(R"({"error": "Unauthorized: Missing or invalid token"})");
                return;
            }

            std::string token = std::string(auth_header).substr(7);

            std::string email = url_decode(ctx->GetQuery("email"));

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
                "Decoded email for display name: %s", email.c_str());

            if (email.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Missing email parameter"})");
                return;
            }

            if (!is_valid_email(email)) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid email format"})");
                return;
            }

            auto& db = tsto::database::PublicUsersDB::get_instance();
            if (!db.validate_access_token(email, token)) {
                ctx->set_response_http_code(401);
                cb(R"({"error": "Unauthorized: Invalid token"})");
                return;
            }

            std::string display_name;
            auto& main_db = database::Database::get_instance();

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                "Attempting to get display name for user with email: %s",
                email.c_str());

            bool success = main_db.get_display_name(email, display_name);

            if (success) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "Successfully retrieved display name for user %s: '%s'",
                    email.c_str(), display_name.c_str());

                if (display_name.empty()) {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                        "Display name is empty for user %s, using email as fallback",
                        email.c_str());
                    display_name = email;
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_DATABASE,
                    "Failed to retrieve display name from database for user %s, using email as fallback",
                    email.c_str());
                display_name = email;     
            }

            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("success", true, doc.GetAllocator());
            doc.AddMember("display_name", rapidjson::Value(display_name.c_str(), doc.GetAllocator()), doc.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            ctx->set_response_http_code(200);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error getting display name: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_update_display_name(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            const char* auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header == nullptr || std::string(auth_header).substr(0, 7) != "Bearer ") {
                ctx->set_response_http_code(401);
                cb(R"({"error": "Unauthorized: Missing or invalid token"})");
                return;
            }

            std::string token = std::string(auth_header).substr(7);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc["email"].IsString() ||
                !doc.HasMember("display_name") || !doc["display_name"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string email = doc["email"].GetString();
            std::string display_name = doc["display_name"].GetString();

            if (!is_valid_email(email)) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid email format"})");
                return;
            }

            if (display_name.empty() || display_name.length() > 50) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Display name must be between 1 and 50 characters"})");
                return;
            }

            auto& db = tsto::database::PublicUsersDB::get_instance();
            if (!db.validate_access_token(email, token)) {
                ctx->set_response_http_code(401);
                cb(R"({"error": "Unauthorized: Invalid token"})");
                return;
            }

            if (!db.update_display_name(email, display_name)) {
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to update display name"})");
                return;
            }

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Display name updated successfully"})");

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Display name updated for public user: %s", email.c_str());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error updating display name: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_public_resend_code(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc["email"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string email = doc["email"].GetString();

            if (!is_valid_email(email)) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid email format"})");
                return;
            }

            auto& db = tsto::database::PublicUsersDB::get_instance();
            if (!db.user_exists(email)) {
                ctx->set_response_http_code(404);
                cb(R"({"error": "User not found"})");
                return;
            }

            std::string display_name;
            bool is_verified;
            int64_t registration_date;
            int64_t last_login;
            db.get_user_info(email, display_name, is_verified, registration_date, last_login);

            tsto::login::Login login;
            std::string verification_code = login.handle_tsto_api(email, display_name);

            if (verification_code.empty()) {
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to generate verification code"})");
                return;
            }

            if (!db.update_user_cred(email, verification_code)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                    "Failed to store verification code for user: %s", email.c_str());
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to store verification code"})");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Resent verification code for %s: %s", email.c_str(), verification_code.c_str());

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Verification code has been resent"})");

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Verification code resent for public user: %s", email.c_str());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error resending verification code: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

}
