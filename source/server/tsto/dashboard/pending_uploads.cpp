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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip> 
#include <random>
#include <ctime>
#include <curl/curl.h>

#include "tsto/land/new_land.hpp"
#include "tsto/database/database.hpp"
#include "tsto/database/pending_towns.hpp"
#include "tsto/includes/session.hpp"
#include "headers/response_headers.hpp"
#include "tsto/statistics/statistics.hpp"

namespace tsto::dashboard {

    bool send_town_approval_email(const std::string& email, const std::string& town_name) {
        bool api_enabled = utils::configuration::ReadBoolean("Notifications", "EmailAPIEnabled", true);
        if (!api_enabled) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] Email notifications are disabled, skipping notification for %s with town %s",
                email.c_str(), town_name.c_str());
            return true;            
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[PUBLIC_UPLOADER] Starting email notification process for %s with town %s",
            email.c_str(), town_name.c_str());

        std::string username = email.substr(0, email.find('@'));

        std::string api_key = "cJs7b3BstxJm6lYpEDhH8hPS9N4n5JU9ofvcpcPduZ6USruxThBrEB4i62BQQxVE";

        auto url_encode = [](const std::string& value) -> std::string {
            std::ostringstream escaped;
            escaped.fill('0');
            escaped << std::hex;

            for (char c : value) {
                if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    escaped << c;
                }
                else if (c == ' ') {
                    escaped << '+';
                }
                else {
                    escaped << '%' << std::setw(2) << int((unsigned char)c);
                }
            }

            return escaped.str();
        };

        std::string url = "https://api.tsto.app/api/town/uploadSuccess?apikey=" + url_encode(api_key) +
            "&emailAddress=" + url_encode(email) +
            "&username=" + url_encode(username) +
            "&town=" + url_encode(town_name);

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
            "[PUBLIC_UPLOADER] Sending approval notification to %s with URL: %s",
            email.c_str(), url.c_str());

        static std::once_flag curl_init_flag;
        std::call_once(curl_init_flag, []() {
            curl_global_init(CURL_GLOBAL_ALL);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] cURL initialized globally");
        });

        try {
            CURL* curl = curl_easy_init();
            if (!curl) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PUBLIC_UPLOADER] Failed to initialize curl handle");
                return false;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] cURL handle initialized successfully");

            curl_easy_setopt(curl, CURLOPT_DNS_CACHE_TIMEOUT, 120L);       
            curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);         
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);                
            curl_easy_setopt(curl, CURLOPT_IPRESOLVE, CURL_IPRESOLVE_V4);        

            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L);    
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");        
            
            static auto write_callback = [](char* ptr, size_t size, size_t nmemb, void* userdata) -> size_t {
                return size * nmemb;
            };
            
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
            
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            
            curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
            
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L);

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] cURL options set, about to perform request");

            CURLcode res = curl_easy_perform(curl);
            
            long response_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] cURL request completed with result: %d (%s), HTTP code: %ld",
                res, curl_easy_strerror(res), response_code);

            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PUBLIC_UPLOADER] curl_easy_perform() failed: %s", curl_easy_strerror(res));
                
                if (res == CURLE_COULDNT_RESOLVE_HOST) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[PUBLIC_UPLOADER] DNS resolution failed. Check network connectivity or DNS settings.");
                } else if (res == CURLE_OPERATION_TIMEDOUT) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[PUBLIC_UPLOADER] Request timed out. The API server might be down or network is slow.");
                }
                
                return false;
            }

            if (response_code >= 200 && response_code < 300) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[PUBLIC_UPLOADER] Email notification sent successfully to %s", email.c_str());
                return true;
            } else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PUBLIC_UPLOADER] API returned error HTTP code: %ld", response_code);
                return false;
            }
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] Exception during email notification: %s", e.what());
            return false;
        }
        catch (...) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] Unknown exception during email notification");
            return false;
        }
    }

    void handle_public_town_uploader(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] Serving public town uploader page to: %s", std::string(ctx->remote_ip()).c_str());
            
            std::ifstream file("webpanel/public_townuploader.html");
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PUBLIC_UPLOADER] Failed to open public_townuploader.html");
                ctx->set_response_http_code(404);
                cb("File not found");
                return;
            }

            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string content = buffer.str();
            
            ctx->AddResponseHeader("Content-Type", "text/html; charset=utf-8");
            cb(content);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] Exception serving public town uploader page: %s", ex.what());
            ctx->set_response_http_code(500);
            headers::set_json_response(ctx);
            cb("{\"success\":false,\"error\":\"Internal server error\"}");
        }
    }

    void handle_submit_town_upload(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] Received town upload submission from: %s", std::string(ctx->remote_ip()).c_str());

            std::string content_type = ctx->FindRequestHeader("Content-Type");
            if (content_type.find("multipart/form-data") == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PUBLIC_UPLOADER] Invalid content type: %s", content_type.c_str());
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Invalid content type. Expected multipart/form-data\"}");
                return;
            }

            std::filesystem::create_directories("pending_towns");

            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<> dis(0, 15);

            std::stringstream ss;
            ss << "pending_towns/upload_" << std::time(nullptr) << "_";
            for (int i = 0; i < 16; i++) {
                ss << std::hex << dis(gen);
            }
            ss << ".pb";

            std::string file_path = ss.str();

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
                    "[PUBLIC_UPLOADER] No boundary found in content type");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Invalid multipart form data format\"}");
                return;
            }

            std::string full_boundary = "--" + boundary;
            std::string body_str = body.ToString();

            std::string email;
            std::string town_name;
            std::string device_id;
            std::string description;
            std::string file_data;

            size_t email_pos = body_str.find("name=\"email\"");
            if (email_pos != std::string::npos) {
                size_t email_data_start = body_str.find("\r\n\r\n", email_pos) + 4;
                size_t email_data_end = body_str.find(full_boundary, email_data_start) - 2;
                email = body_str.substr(email_data_start, email_data_end - email_data_start);
            }

            size_t town_name_pos = body_str.find("name=\"town_name\"");
            if (town_name_pos != std::string::npos) {
                size_t town_name_data_start = body_str.find("\r\n\r\n", town_name_pos) + 4;
                size_t town_name_data_end = body_str.find(full_boundary, town_name_data_start) - 2;
                town_name = body_str.substr(town_name_data_start, town_name_data_end - town_name_data_start);
            }

            const char* user_agent = ctx->FindRequestHeader("User-Agent");
            if (user_agent) {
                std::string agent_str(user_agent);
                std::hash<std::string> hasher;
                device_id = std::to_string(hasher(agent_str));
            } else {
                device_id = std::string(ctx->remote_ip()) + "_" + std::to_string(std::time(nullptr));
            }

            size_t description_pos = body_str.find("name=\"description\"");
            if (description_pos != std::string::npos) {
                size_t description_data_start = body_str.find("\r\n\r\n", description_pos) + 4;
                size_t description_data_end = body_str.find(full_boundary, description_data_start) - 2;
                description = body_str.substr(description_data_start, description_data_end - description_data_start);
            }

            size_t file_pos = body_str.find("name=\"town_file\"");
            if (file_pos != std::string::npos) {
                size_t file_headers_end = body_str.find("\r\n\r\n", file_pos);
                if (file_headers_end == std::string::npos) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[PUBLIC_UPLOADER] Could not find end of file headers");
                    ctx->set_response_http_code(400);
                    headers::set_json_response(ctx);
                    cb("{\"success\":false,\"error\":\"Invalid multipart form data format\"}");
                    return;
                }

                size_t filename_pos = body_str.find("filename=\"", file_pos);
                if (filename_pos != std::string::npos) {
                    size_t filename_end = body_str.find("\"", filename_pos + 10);
                    if (filename_end != std::string::npos) {
                        std::string filename = body_str.substr(filename_pos + 10, filename_end - (filename_pos + 10));
                        
                        size_t dot_pos = filename.find_last_of(".");
                        if (dot_pos == std::string::npos || filename.substr(dot_pos) != ".pb") {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                                "[PUBLIC_UPLOADER] Invalid file extension: %s", filename.c_str());
                            ctx->set_response_http_code(400);
                            headers::set_json_response(ctx);
                            cb("{\"success\":false,\"error\":\"Invalid file type. Only .pb files are allowed.\"}");
                            return;
                        }
                    }
                }

                size_t file_data_start = file_headers_end + 4;
                size_t file_data_end = body_str.find(full_boundary, file_data_start);
                if (file_data_end == std::string::npos) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[PUBLIC_UPLOADER] Could not find end boundary");
                    ctx->set_response_http_code(400);
                    headers::set_json_response(ctx);
                    cb("{\"success\":false,\"error\":\"Invalid multipart form data format\"}");
                    return;
                }

                if (file_data_end > 2) {
                    file_data_end -= 2;
                }

                file_data = body_str.substr(file_data_start, file_data_end - file_data_start);
            }

            if (email.empty() || file_data.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PUBLIC_UPLOADER] Missing required fields: Email=%s, FileData=%s",
                    email.empty() ? "missing" : "present",
                    file_data.empty() ? "missing" : "present");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Missing required fields\"}");
                return;
            }

            if (town_name.empty()) {
                size_t at_pos = email.find('@');
                if (at_pos != std::string::npos) {
                    town_name = email.substr(0, at_pos) + "'s Town";
                } else {
                    town_name = "Unknown Town";
                }
            }

            std::ofstream out_file(file_path, std::ios::binary);
            if (!out_file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PUBLIC_UPLOADER] Failed to open file for writing: %s", file_path.c_str());
                ctx->set_response_http_code(500);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Failed to save uploaded file\"}");
                return;
            }

            out_file.write(file_data.c_str(), file_data.size());
            out_file.close();

            auto& pending_towns = tsto::database::PendingTowns::get_instance();
            auto town_id_opt = pending_towns.add_pending_town(
                email,
                town_name,
                description,
                file_path,
                file_data.size()
            );

            if (!town_id_opt) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PUBLIC_UPLOADER] Failed to add pending town to database");
                
                try {
                    std::filesystem::remove(file_path);
                } catch (...) {
                }
                
                ctx->set_response_http_code(500);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Failed to register town upload\"}");
                return;
            }

            tsto::statistics::Statistics::get_instance().record_public_town_submission(device_id);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] Successfully saved pending town upload: ID=%s, Email=%s, TownName=%s, FilePath=%s",
                town_id_opt.value().c_str(), email.c_str(), town_name.c_str(), file_path.c_str());

            headers::set_json_response(ctx);
            cb("{\"success\":true,\"message\":\"Town uploaded successfully. It will be reviewed by an administrator.\"}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PUBLIC_UPLOADER] Exception handling town upload: %s", ex.what());
            ctx->set_response_http_code(500);
            headers::set_json_response(ctx);
            cb("{\"success\":false,\"error\":\"Internal server error: " + std::string(ex.what()) + "\"}");
        }
    }

    void handle_get_pending_towns(evpp::EventLoop* loop, const std::shared_ptr<evpp::http::Context>& ctx,
        const std::function<void(const std::string&)>& cb) {
        if (!Dashboard::is_authenticated(ctx)) {
            Dashboard::redirect_to_login(ctx, cb);
            return;
        }

        if (!Dashboard::has_town_operations_access(ctx)) {
            Dashboard::redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[PENDING_TOWNS] Getting pending town uploads for: %s", std::string(ctx->remote_ip()).c_str());

            auto& pending_towns = tsto::database::PendingTowns::get_instance();
            
            pending_towns.initialize();
            
            auto towns = pending_towns.get_pending_towns();

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            rapidjson::Value towns_array(rapidjson::kArrayType);

            for (const auto& town : towns) {
                rapidjson::Value town_obj(rapidjson::kObjectType);
                
                town_obj.AddMember("id", rapidjson::Value(town.id.c_str(), allocator), allocator);
                town_obj.AddMember("email", rapidjson::Value(town.email.c_str(), allocator), allocator);
                town_obj.AddMember("town_name", rapidjson::Value(town.town_name.c_str(), allocator), allocator);
                town_obj.AddMember("description", rapidjson::Value(town.description.c_str(), allocator), allocator);
                town_obj.AddMember("file_path", rapidjson::Value(town.file_path.c_str(), allocator), allocator);
                town_obj.AddMember("file_size", rapidjson::Value(static_cast<uint64_t>(town.file_size)), allocator);
                
                town_obj.AddMember("submitted_at", rapidjson::Value(town.timestamp_str.c_str(), allocator), allocator);
                
                town_obj.AddMember("status", rapidjson::Value(town.status.c_str(), allocator), allocator);
                
                if (!town.rejection_reason.empty()) {
                    town_obj.AddMember("rejection_reason", rapidjson::Value(town.rejection_reason.c_str(), allocator), allocator);
                }
                else {
                    town_obj.AddMember("rejection_reason", rapidjson::Value("", allocator), allocator);
                }
                
                towns_array.PushBack(town_obj, allocator);
            }

            doc.AddMember("towns", towns_array, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            headers::set_json_response(ctx);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PENDING_TOWNS] Exception getting pending town uploads: %s", ex.what());
            ctx->set_response_http_code(500);
            headers::set_json_response(ctx);
            cb("{\"error\":\"Internal server error: " + std::string(ex.what()) + "\"}");
        }
    }

    void handle_approve_pending_town(evpp::EventLoop* loop, const std::shared_ptr<evpp::http::Context>& ctx,
                                    const std::function<void(const std::string&)>& cb) {
        if (!Dashboard::is_authenticated(ctx)) {
            Dashboard::redirect_to_login(ctx, cb);
            return;
        }

        if (!Dashboard::has_town_operations_access(ctx)) {
            Dashboard::redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            std::string body = ctx->body().ToString();
            
            rapidjson::Document doc;
            if (doc.Parse(body.c_str()).HasParseError()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Invalid JSON in request");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Invalid JSON\"}");
                return;
            }

            std::string username = Dashboard::get_username_from_session(ctx);
            if (username.empty()) {
                username = "unknown";
            }

            if (!doc.HasMember("id") || !doc["id"].IsString()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Missing or invalid 'id' field in request");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Missing or invalid 'id' field\"}");
                return;
            }

            std::string id = doc["id"].GetString();
            std::string target_email;

            if (doc.HasMember("target_email") && doc["target_email"].IsString()) {
                target_email = doc["target_email"].GetString();
            }

            auto& pending_towns = tsto::database::PendingTowns::get_instance();
            
            std::optional<tsto::database::PendingTownData> town_opt;
            
            try {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_DATABASE,
                    "[PENDING_TOWNS] Retrieved pending town with ID: %s", id.c_str());
                town_opt = pending_towns.get_pending_town(id);
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Exception retrieving town data: %s", ex.what());
                ctx->set_response_http_code(500);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Error retrieving town data: " + std::string(ex.what()) + "\"}");
                return;
            }
            
            if (!town_opt) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Town not found with ID: %s", id.c_str());
                ctx->set_response_http_code(404);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Town not found\"}");
                return;
            }
            
            std::string email_to_notify = town_opt->email;
            std::string town_name = town_opt->town_name;
            
            bool success = false;
            try {
                success = pending_towns.approve_town(id, target_email);
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Exception approving town: %s", ex.what());
                ctx->set_response_http_code(500);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Error approving town: " + std::string(ex.what()) + "\"}");
                return;
            }

            if (success) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Successfully approved town upload: ID=%s, TargetEmail=%s",
                    id.c_str(), target_email.empty() ? "original" : target_email.c_str());

                tsto::statistics::Statistics::get_instance().record_town_approval(username);
                
                try {
                    send_town_approval_email(email_to_notify, town_name);
                }
                catch (const std::exception& ex) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[PENDING_TOWNS] Error sending approval notification: %s", ex.what());
                }

                headers::set_json_response(ctx);
                cb("{\"success\":true,\"message\":\"Town upload approved successfully\"}");
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Failed to approve town upload: ID=%s", id.c_str());
                ctx->set_response_http_code(500);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Failed to approve town upload\"}");
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PENDING_TOWNS] Exception approving pending town upload: %s", ex.what());
            ctx->set_response_http_code(500);
            headers::set_json_response(ctx);
            cb("{\"success\":false,\"error\":\"Internal server error: " + std::string(ex.what()) + "\"}");
        }
    }

    void handle_reject_pending_town(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_authenticated(ctx)) {
            Dashboard::redirect_to_login(ctx, cb);
            return;
        }

        if (!Dashboard::has_town_operations_access(ctx)) {
            Dashboard::redirect_to_access_denied(ctx, cb);
            return;
        }

        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[PENDING_TOWNS] Rejecting pending town upload from: %s", std::string(ctx->remote_ip()).c_str());

            rapidjson::Document doc;
            doc.Parse(ctx->body().ToString().c_str());

            if (doc.HasParseError() || !doc.IsObject()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Invalid JSON in request body");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Invalid JSON in request body\"}");
                return;
            }

            if (!doc.HasMember("id") || !doc["id"].IsString()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Missing or invalid 'id' field in request");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Missing or invalid 'id' field\"}");
                return;
            }

            std::string id = doc["id"].GetString();
            std::string reason;

            if (doc.HasMember("reason") && doc["reason"].IsString()) {
                reason = doc["reason"].GetString();
            }

            auto& pending_towns = tsto::database::PendingTowns::get_instance();
            bool success = pending_towns.reject_town(id, reason);

            if (success) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Successfully rejected town upload: ID=%s, Reason=%s",
                    id.c_str(), reason.empty() ? "None provided" : reason.c_str());

                headers::set_json_response(ctx);
                cb("{\"success\":true,\"message\":\"Town upload rejected successfully\"}");
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Failed to reject town upload: ID=%s", id.c_str());
                ctx->set_response_http_code(500);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Failed to reject town upload\"}");
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PENDING_TOWNS] Exception rejecting pending town upload: %s", ex.what());
            ctx->set_response_http_code(500);
            headers::set_json_response(ctx);
            cb("{\"success\":false,\"error\":\"Internal server error: " + std::string(ex.what()) + "\"}");
        }
    }

    void handle_get_user_pending_towns(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[PENDING_TOWNS] Getting user pending town uploads from: %s", std::string(ctx->remote_ip()).c_str());

            std::string query = ctx->uri();
            size_t pos = query.find("?email=");
            if (pos == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PENDING_TOWNS] Missing email parameter");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"error\":\"Missing email parameter\"}");
                return;
            }

            std::string email = query.substr(pos + 7);
            
            std::string decoded_email = email;
            size_t decode_pos = 0;
            while ((decode_pos = decoded_email.find('+', decode_pos)) != std::string::npos) {
                decoded_email.replace(decode_pos, 1, " ");
                decode_pos++;
            }
            
            auto& pending_towns = tsto::database::PendingTowns::get_instance();
            auto towns = pending_towns.get_pending_towns_by_email(decoded_email);

            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            rapidjson::Value towns_array(rapidjson::kArrayType);

            for (const auto& town : towns) {
                rapidjson::Value town_obj(rapidjson::kObjectType);
                
                town_obj.AddMember("id", rapidjson::Value(town.id.c_str(), allocator), allocator);
                town_obj.AddMember("town_name", rapidjson::Value(town.town_name.c_str(), allocator), allocator);
                
                town_obj.AddMember("submitted_at", rapidjson::Value(town.timestamp_str.c_str(), allocator), allocator);
                
                town_obj.AddMember("status", rapidjson::Value(town.status.c_str(), allocator), allocator);
                
                if (!town.rejection_reason.empty()) {
                    town_obj.AddMember("rejection_reason", rapidjson::Value(town.rejection_reason.c_str(), allocator), allocator);
                }
                else {
                    town_obj.AddMember("rejection_reason", rapidjson::Value("", allocator), allocator);
                }
                
                towns_array.PushBack(town_obj, allocator);
            }

            doc.AddMember("towns", towns_array, allocator);
            doc.AddMember("success", true, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            headers::set_json_response(ctx);
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PENDING_TOWNS] Exception getting user pending town uploads: %s", ex.what());
            ctx->set_response_http_code(500);
            headers::set_json_response(ctx);
            cb("{\"success\":false,\"error\":\"Internal server error: " + std::string(ex.what()) + "\"}");
        }
    }

}   
