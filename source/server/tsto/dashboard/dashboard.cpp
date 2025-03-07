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

#include "tsto/land/land.hpp"
#include "tsto/events/events.hpp"
#include "tsto/database/database.hpp"
#include "tsto/includes/session.hpp"
#include "headers/response_headers.hpp"

namespace tsto::dashboard {

    // Initialize static members
    std::string Dashboard::server_ip_;
    uint16_t Dashboard::server_port_;

    void Dashboard::handle_server_restart(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        ctx->AddResponseHeader("Content-Type", "application/json");

        cb("{\"status\":\"success\",\"message\":\"Server restart initiated\"}");

        loop->RunAfter(evpp::Duration(1.0), []() {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Server stopping for restart...");

            char exePath[MAX_PATH];
            GetModuleFileNameA(NULL, exePath, MAX_PATH);

            STARTUPINFOA si = { sizeof(STARTUPINFOA) };
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

    void Dashboard::handle_dashboard(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

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

            static auto start_time = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
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

    void Dashboard::handle_server_stop(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        ctx->AddResponseHeader("Content-Type", "application/json");

        cb("{\"status\":\"success\",\"message\":\"Server shutdown initiated\"}");

        loop->RunAfter(evpp::Duration(1.0), []() {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                "Server stopping...");
            ExitProcess(0);
            });
    }

    void Dashboard::handle_update_initial_donuts(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.HasMember("initialDonuts") || !doc["initialDonuts"].IsInt()) {
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid request body\"}");
                return;
            }

            int initial_donuts = doc["initialDonuts"].GetInt();
            utils::configuration::WriteString("Server", "InitialDonutAmount", std::to_string(initial_donuts));

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[DONUTS] Updated initial donuts amount to: %d", initial_donuts);

            headers::set_json_response(ctx);
            cb("{\"success\": true}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[DONUTS] Error updating initial donuts: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\": \"Internal server error\"}");
        }
    }

    void Dashboard::handle_force_save_protoland(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            auto& session = tsto::Session::get();

            if (!session.land_proto.IsInitialized()) {
                ctx->AddResponseHeader("Content-Type", "application/json");
                ctx->set_response_http_code(400);
                cb("{\"status\":\"error\",\"message\":\"No land data to save\"}");
                return;
            }

            if (tsto::land::Land::save_town()) {
                ctx->AddResponseHeader("Content-Type", "application/json");
                cb("{\"status\":\"success\"}");
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[LAND] Force saved town successfully");
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
        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.IsObject() || !doc.HasMember("email") || !doc.HasMember("amount") || 
                !doc["email"].IsString() || !doc["amount"].IsInt()) {
                throw std::runtime_error("Invalid request. Required fields: email (string), amount (integer)");
            }

            std::string email = doc["email"].GetString();
            int amount = doc["amount"].GetInt();

            //clean up the email/username string
            size_t pb_pos = email.find(".pb");
            if (pb_pos != std::string::npos) {
                email = email.substr(0, pb_pos);
            }
            size_t txt_pos = email.find(".txt");
            if (txt_pos != std::string::npos) {
                email = email.substr(0, txt_pos);
            }

            //unique currency file for each user
            std::string currency_file;
            if (email == "mytown") {
                currency_file = "towns/currency.txt"; //legacy default path
            } else {
                currency_file = "towns/currency_" + email + ".txt";
            }

            std::filesystem::create_directories("towns");

            std::ofstream output(currency_file);
            if (!output.good()) {
                throw std::runtime_error("Failed to open currency file for writing");
            }
            output << amount;
            output.close();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[CURRENCY] Updated currency for user %s to %d donuts (file: %s)",
                email.c_str(), amount, currency_file.c_str());

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
        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[UPLOAD] Received town file upload request from: %s", std::string(ctx->remote_ip()).c_str());
            
            std::string content_type = ctx->FindRequestHeader("Content-Type");
            if (content_type.find("multipart/form-data") == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Invalid content type: %s", content_type.c_str());
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"message\":\"Invalid content type. Expected multipart/form-data\"}");
                return;
            }
            
            const char* auth_header = ctx->FindRequestHeader("nucleus_token");
            if (auth_header && strlen(auth_header) > 0) {
                auto& db = tsto::database::Database::get_instance();
                std::string token_email;
                
                if (db.validate_access_token(auth_header, token_email)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[UPLOAD] Valid token for user: %s", token_email.c_str());
                } else {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
                        "[UPLOAD] Invalid token provided");
                    //still allow the upload for now, just log the warning
                }
            } else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[UPLOAD] No authentication token provided");
            }
            
            // Create a temporary directory if it doesn't exist
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
            } else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] No boundary found in content type");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"message\":\"Invalid multipart form data format\"}");
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
                cb("{\"success\":false,\"message\":\"Invalid multipart form data format\"}");
                return;
            }
            
            pos = body_str.find("Content-Disposition:", pos);
            if (pos == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Could not find Content-Disposition header");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"message\":\"Invalid multipart form data format\"}");
                return;
            }
            
            size_t headers_end = body_str.find("\r\n\r\n", pos);
            if (headers_end == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Could not find end of headers");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"message\":\"Invalid multipart form data format\"}");
                return;
            }
            
            size_t data_start = headers_end + 4;
            
            size_t data_end = body_str.find(full_boundary, data_start);
            if (data_end == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Could not find end boundary");
                ctx->set_response_http_code(400);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"message\":\"Invalid multipart form data format\"}");
                return;
            }
            
            if (data_end > 2) {
                data_end -= 2;
            }
            
            std::string file_data = body_str.substr(data_start, data_end - data_start);
            
            std::ofstream out_file(temp_file_path, std::ios::binary);
            if (!out_file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[UPLOAD] Failed to open temporary file for writing: %s", temp_file_path.c_str());
                ctx->set_response_http_code(500);
                headers::set_json_response(ctx);
                cb("{\"success\":false,\"message\":\"Failed to save uploaded file\"}");
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
            cb("{\"success\":false,\"message\":\"Internal server error: " + std::string(ex.what()) + "\"}");
        }
    }

    void Dashboard::handle_list_users(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
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
            auto& allocator = response.GetAllocator();

            rapidjson::Value users_array(rapidjson::kArrayType);
            for (const auto& town_file : town_files) {
                rapidjson::Value user_obj(rapidjson::kObjectType);
                
                std::string username = town_file;
                size_t pos = username.find(".pb");
                if (pos != std::string::npos) {
                    username = username.substr(0, pos);
                }

                //currency using the same logic as handle_edit_user_currency
                std::string currency_file;
                if (username == "mytown") {
                    currency_file = "towns/currency.txt";
                } else {
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

                user_obj.AddMember("username", rapidjson::Value(username.c_str(), allocator), allocator);
                user_obj.AddMember("townFile", rapidjson::Value(town_file.c_str(), allocator), allocator);
                user_obj.AddMember("currency", currency, allocator);
                user_obj.AddMember("isLegacy", username == "mytown", allocator);

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
                    cb("{\"error\": \"Invalid JSON in request\"}");
                    return;
                }
    
                if (!request.HasMember("username") || !request["username"].IsString()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Missing username in request");
                    ctx->set_response_http_code(400);
                    cb("{\"error\": \"Missing username\"}");
                    return;
                }
    
                username = request["username"].GetString();
                isLegacy = request.HasMember("isLegacy") && request["isLegacy"].IsBool() && request["isLegacy"].GetBool();
            } else {
                const std::string& uri = ctx->original_uri();
                size_t pos = uri.find("username=");
                if (pos == std::string::npos) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Missing username parameter");
                    ctx->set_response_http_code(400);
                    cb("{\"error\": \"Missing username parameter\"}");
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
                        int value = 0;
                        std::istringstream is(username.substr(i + 1, 2));
                        if (is >> std::hex >> value) {
                            decoded += static_cast<char>(value);
                            i += 2;
                        } else {
                            decoded += username[i];
                        }
                    } else if (username[i] == '+') {
                        decoded += ' ';
                    } else {
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
                        cb("{\"error\": \"Failed to create towns directory\"}");
                        return;
                    }
                }
    
                std::filesystem::path savePath = townsDir;
                if (isLegacy) {
                    savePath /= "mytown.pb";
                } else {
                    savePath /= (username + ".pb");
                }
    
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, 
                    "Attempting to read save file: %s", savePath.string().c_str());
    
                if (!std::filesystem::exists(savePath)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                        "Save file does not exist: %s", savePath.string().c_str());
                    ctx->set_response_http_code(404);
                    cb("{\"error\": \"Save file not found\"}");
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
                    cb("{\"error\": \"Failed to parse save file\"}");
                    return;
                }

                // Convert protobuf to JSON
                std::string json_string;
                google::protobuf::util::JsonPrintOptions options;
                options.add_whitespace = true;
                options.preserve_proto_field_names = true;
                
                auto status = google::protobuf::util::MessageToJsonString(save_data, &json_string, options);
                if (!status.ok()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                        "Failed to convert save to JSON for user: %s - %s", username.c_str(), status.ToString().c_str());
                    ctx->set_response_http_code(500);
                    cb("{\"error\": \"Failed to convert save to JSON\"}");
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
            } catch (const std::exception& e) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                    "Exception while loading save: %s", e.what());
                ctx->set_response_http_code(500);
                cb(std::string("{\"error\": \"Failed to read save file: ") + e.what() + "\"}");
            }
        } catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                "Exception in handle_get_user_save: %s", e.what());
            ctx->set_response_http_code(500);
            cb(std::string("{\"error\": \"Server error: ") + e.what() + "\"}");
        }
    }

    void Dashboard::handle_save_user_save(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        ctx->AddResponseHeader("Content-Type", "application/json");
        
        try {
            std::string requestBody = std::string(ctx->body().data(), ctx->body().size());
            //logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, 
            //    "Received save request body: %s", requestBody.c_str());
    
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
                
                // Convert JSON back to protobuf
                auto status = google::protobuf::util::JsonStringToMessage(json_string, &save_data);
                if (!status.ok()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                        "Failed to parse JSON for user: %s - %s", username.c_str(), status.ToString().c_str());
                    ctx->set_response_http_code(400);
                    cb("{\"error\": \"Invalid save data format\"}");
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
                        cb("{\"error\": \"Failed to create towns directory\"}");
                        return;
                    }
                }

                if (isLegacy) {
                    savePath /= "mytown.pb";
                } else {
                    savePath /= (username + ".pb");
                }

                //backup
                if (std::filesystem::exists(savePath)) {
                    std::filesystem::path backupPath = savePath;
                    backupPath += ".bak";
                    std::filesystem::copy_file(savePath, backupPath, std::filesystem::copy_options::overwrite_existing);
                }

                //save the protobuf data using the same format as the game's load
                std::string serialized;
                if (!save_data.SerializeToString(&serialized)) {
                    throw std::runtime_error("Failed to serialize protobuf data");
                }

                //to vector<char> like the game uses
                std::vector<char> buffer(serialized.begin(), serialized.end());

                std::ofstream out(savePath, std::ios::binary);
                if (!out.is_open()) {
                    throw std::runtime_error("Failed to open save file for writing");
                }

                out.write(buffer.data(), buffer.size());
                out.close();

                //verify we can read it back
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
                cb("{\"success\": true}");
            } catch (const std::exception& e) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                    "Exception while saving: %s", e.what());
                ctx->set_response_http_code(500);
                cb(std::string("{\"error\": \"Failed to save file: ") + e.what() + "\"}");
            }
        } catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                "Exception in handle_save_user_save: %s", e.what());
            ctx->set_response_http_code(500);
            cb(std::string("{\"error\": \"Server error: ") + e.what() + "\"}");
        }
    }

    void Dashboard::handle_dashboard_data(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        ctx->AddResponseHeader("Content-Type", "application/json");
        ctx->AddResponseHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
        ctx->AddResponseHeader("Pragma", "no-cache");
        ctx->AddResponseHeader("Expires", "0");

        try {
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            //server IP and port
            doc.AddMember("server_ip", rapidjson::Value(server_ip_.c_str(), allocator), allocator);
            doc.AddMember("server_port", server_port_, allocator);

            //uptime
            static auto start_time = std::chrono::system_clock::now();
            auto now = std::chrono::system_clock::now();
            auto uptime = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
            auto hours = std::chrono::duration_cast<std::chrono::hours>(uptime);
            auto minutes = std::chrono::duration_cast<std::chrono::minutes>(uptime % std::chrono::hours(1));
            auto seconds = uptime % std::chrono::minutes(1);
            std::stringstream uptime_str;
            uptime_str << hours.count() << "h " << minutes.count() << "m " << seconds.count() << "s";
            doc.AddMember("uptime", rapidjson::Value(uptime_str.str().c_str(), allocator), allocator);

            //DLC directory
            std::string dlc_directory = utils::configuration::ReadString("Server", "DLCDirectory", "dlc");
            doc.AddMember("dlc_directory", rapidjson::Value(dlc_directory.c_str(), allocator), allocator);

            //initial donuts
            std::string initial_donuts = utils::configuration::ReadString("Server", "InitialDonutAmount", "1000");
            doc.AddMember("initial_donuts", rapidjson::Value(initial_donuts.c_str(), allocator), allocator);

            //current event
            auto current_event = tsto::events::Events::get_current_event();
            doc.AddMember("current_event", rapidjson::Value(current_event.name.c_str(), allocator), allocator);
            doc.AddMember("current_event_time", current_event.start_time, allocator);

            //events list
            rapidjson::Value events_obj(rapidjson::kObjectType);
            for (const auto& event_pair : tsto::events::tsto_events) {
                rapidjson::Value key(std::to_string(event_pair.first).c_str(), allocator);
                rapidjson::Value value(event_pair.second.c_str(), allocator);
                events_obj.AddMember(key, value, allocator);
            }
            doc.AddMember("events", events_obj, allocator);

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

}
