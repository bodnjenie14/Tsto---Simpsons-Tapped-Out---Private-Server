#include <std_include.hpp>
#include "admin_town_management.hpp"
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

namespace tsto::dashboard {

    namespace {
        void set_json_response(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->AddResponseHeader("Server", "TSTO-Server/1.0");
            ctx->AddResponseHeader("Cache-Control", "no-store, no-cache, must-revalidate");
            ctx->AddResponseHeader("Access-Control-Allow-Origin", "*");
            ctx->AddResponseHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
            ctx->AddResponseHeader("Access-Control-Allow-Headers", "Content-Type, Authorization");
        }

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
    }

    void handle_admin_import_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string target_user_email = url_decode(ctx->GetQuery("target_user"));

            if (target_user_email.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Target user email is required"})");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Admin is importing a town for user %s", target_user_email.c_str());

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

            std::string dest_path = "towns/" + target_user_email + ".pb";

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
                "Admin successfully imported town for user %s to %s",
                target_user_email.c_str(), dest_path.c_str());

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Town imported successfully"})");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error in admin town import: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_admin_export_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string target_user_email = url_decode(ctx->GetQuery("target_user"));

            if (target_user_email.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Target user email is required"})");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Admin is exporting town for user %s", target_user_email.c_str());

            std::string town_file_path = "towns/" + target_user_email + ".pb";

            if (!std::filesystem::exists(town_file_path)) {
                ctx->set_response_http_code(404);
                cb(R"({"error": "Town not found"})");
                return;
            }

            std::ifstream file(town_file_path, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Failed to open town file: %s", town_file_path.c_str());
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to open town file"})");
                return;
            }

            file.seekg(0, std::ios::end);
            size_t file_size = file.tellg();
            file.seekg(0, std::ios::beg);

            std::vector<char> buffer(file_size);
            file.read(buffer.data(), file_size);
            file.close();

            ctx->AddResponseHeader("Content-Type", "application/octet-stream");
            ctx->AddResponseHeader("Content-Disposition", "attachment; filename=\"" + target_user_email + ".pb\"");
            ctx->AddResponseHeader("Content-Length", std::to_string(file_size));

            ctx->set_response_http_code(200);
            cb(std::string(buffer.data(), file_size));

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Admin successfully exported town for user %s",
                target_user_email.c_str());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error in admin town export: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_admin_delete_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("target_user") || !doc["target_user"].IsString()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string target_user_email = doc["target_user"].GetString();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Admin is deleting town for user %s", target_user_email.c_str());

            std::string town_file_path = "towns/" + target_user_email + ".pb";

            if (!std::filesystem::exists(town_file_path)) {
                ctx->set_response_http_code(404);
                cb(R"({"error": "Town not found"})");
                return;
            }

            std::error_code ec;
            if (!std::filesystem::remove(town_file_path, ec)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Failed to delete town file: %s, error: %s",
                    town_file_path.c_str(), ec.message().c_str());
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to delete town file"})");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Admin successfully deleted town for user %s",
                target_user_email.c_str());

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Town deleted successfully"})");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error in admin town deletion: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_admin_save_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("target_user") ||
                !doc.HasMember("donuts") || !doc["target_user"].IsString() ||
                !doc["donuts"].IsInt()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Invalid request format"})");
                return;
            }

            std::string target_user_email = doc["target_user"].GetString();
            int donuts = doc["donuts"].GetInt();

            const int MAX_DONUTS = 100000;
            if (donuts < 0) {
                donuts = 0;
            }
            else if (donuts > MAX_DONUTS) {
                donuts = MAX_DONUTS;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Admin is setting %d donuts for user %s",
                donuts, target_user_email.c_str());

            std::filesystem::create_directories("towns");

            std::string currency_file_path = "towns/" + target_user_email + ".txt";

            std::ofstream file(currency_file_path);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Failed to create currency file: %s", currency_file_path.c_str());
                ctx->set_response_http_code(500);
                cb(R"({"error": "Failed to create currency file"})");
                return;
            }

            file << donuts;
            file.close();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Admin successfully set %d donuts for user %s",
                donuts, target_user_email.c_str());

            ctx->set_response_http_code(200);
            cb(R"({"success": true, "message": "Currency saved successfully"})");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error in admin currency save: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_admin_get_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            std::string target_user_email = url_decode(ctx->GetQuery("target_user"));

            if (target_user_email.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Target user email is required"})");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Admin is getting currency for user %s", target_user_email.c_str());

            std::string currency_file_path = "towns/" + target_user_email + ".txt";
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
                        "Found currency file for %s at %s with %d donuts", 
                        target_user_email.c_str(), currency_file_path.c_str(), donuts);
                }
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();

            response.AddMember("success", true, allocator);
            response.AddMember("hasCurrency", has_currency, allocator);
            response.AddMember("donuts", donuts, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->set_response_http_code(200);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error in admin get currency: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

    void handle_admin_view_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            set_json_response(ctx);

            if (!Dashboard::is_admin(ctx)) {
                ctx->set_response_http_code(403);
                cb(R"({"error": "Unauthorized: Admin access required"})");
                return;
            }

            std::string target_user_email = url_decode(ctx->GetQuery("email"));

            if (target_user_email.empty()) {
                ctx->set_response_http_code(400);
                cb(R"({"error": "Target user email is required"})");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Admin is viewing dashboard for user %s", target_user_email.c_str());

            std::string temp_token = "admin_view_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();

            response.AddMember("success", true, allocator);
            response.AddMember("email", rapidjson::Value(target_user_email.c_str(), allocator), allocator);
            response.AddMember("token", rapidjson::Value(temp_token.c_str(), allocator), allocator);
            response.AddMember("is_admin_view", true, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->set_response_http_code(200);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                "Error in admin view user: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"error": "Internal server error"})");
        }
    }

}   
