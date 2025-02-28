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
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iomanip> 

#include "tsto/land/land.hpp"
#include "tsto/events/events.hpp"

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

            int current_donuts = std::stoi(utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));

            if (std::filesystem::exists(currency_path)) {
                std::ifstream input(currency_path);
                if (input.good()) {
                    input >> current_donuts;
                }
                input.close();
            }

            html_template = std::regex_replace(html_template, std::regex("%CURRENT_DONUTS%"), std::to_string(current_donuts));
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

    void Dashboard::handle_update_current_donuts(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (!doc.HasMember("currentDonuts") || !doc["currentDonuts"].IsInt()) {
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid request body\"}");
                return;
            }

            int current_donuts = doc["currentDonuts"].GetInt();
            std::string currency_path = "towns/currency.txt";

            std::filesystem::create_directories("towns");
            std::ofstream output(currency_path);
            output << current_donuts;
            output.close();

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[DONUTS] Updated current donuts amount to: %d", current_donuts);

            headers::set_json_response(ctx);
            cb("{\"success\": true}");
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[DONUTS] Error updating current donuts: %s", ex.what());
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
            response.AddMember("email", rapidjson::StringRef(email.c_str()), response.GetAllocator());
            response.AddMember("amount", amount, response.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[CURRENCY] Error updating currency: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("{\"error\": \"Failed to update currency\"}");
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
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "Handling get-user-save request");
        
        std::string requestBody = std::string(ctx->body().data(), ctx->body().size());
        
        // logger had me off again
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "Request body:");
        logger::write("tsto_server.log", requestBody + "\n");
        
        std::string username;
        bool isLegacy = false;

        if (!requestBody.empty()) {
            rapidjson::Document request;
            if (request.Parse(requestBody.c_str()).HasParseError()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Failed to parse request JSON");
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid JSON in request\"}");
                return;
            }

            if (!request.HasMember("username")) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Missing username in request");
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Missing username\"}");
                return;
            }

            username = request["username"].GetString();
            isLegacy = request.HasMember("isLegacy") && request["isLegacy"].GetBool();
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
                std::filesystem::create_directory(townsDir);
            }

            std::filesystem::path savePath = townsDir;
            if (isLegacy) {
                savePath /= "mytown.pb";
            } else {
                savePath /= (username + ".pb");
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, 
                "Attempting to read save file: %s", savePath.string().c_str());

            std::ifstream in(savePath, std::ios::binary);
            if (!in.is_open()) {
                throw std::runtime_error("Failed to open save file for reading");
            }

            Data::LandMessage save_data;
            if (!save_data.ParseFromIstream(&in)) {
                throw std::runtime_error("Failed to parse protobuf save file");
            }
            in.close();

            std::string text_format;
            google::protobuf::TextFormat::PrintToString(save_data, &text_format);

            rapidjson::Document response;
            response.SetObject();
            auto& allocator = response.GetAllocator();

            response.AddMember("status", "success", allocator);
            response.AddMember("save", rapidjson::StringRef(text_format.c_str()), allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, 
                "Successfully loaded save file for user: %s", username.c_str());

            ctx->set_response_http_code(200);
            cb(buffer.GetString());

        } catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                "Exception while loading save: %s", e.what());
            ctx->set_response_http_code(500);
            cb(std::string(R"({"error": "Failed to read save file: )") + e.what() + "\"}");
        }
    }

    void Dashboard::handle_save_user_save(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        ctx->AddResponseHeader("Content-Type", "application/json");
        
        std::string requestBody = std::string(ctx->body().data(), ctx->body().size());
        
        //lol logger had me off here 
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "Received save request body:");
        logger::write("tsto_server.log", requestBody + "\n");

        rapidjson::Document request;
        if (request.Parse(requestBody.c_str()).HasParseError()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Failed to parse request JSON");
            ctx->set_response_http_code(400);
            cb("{\"error\": \"Invalid JSON in request\"}");
            return;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, 
            "Request has fields - username: %s, isLegacy: %s, save: %s", 
            request.HasMember("username") ? "true" : "false",
            request.HasMember("isLegacy") ? "true" : "false",
            request.HasMember("save") ? "true" : "false");

        if (!request.HasMember("username") || !request.HasMember("isLegacy") || !request.HasMember("save")) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Missing required fields in request");
            ctx->set_response_http_code(400);
            cb("{\"error\": \"Missing required fields\"}");
            return;
        }

        std::string usernamePresent = request.HasMember("username") ? request["username"].GetString() : "";
        std::string isLegacyStr = request.HasMember("isLegacy") ? (request["isLegacy"].GetBool() ? "true" : "false") : "false";
        std::string hasSaveStr = request.HasMember("save") ? "true" : "false";
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, 
            "Request has fields - username: %s, isLegacy: %s, save: %s", 
            usernamePresent.c_str(), isLegacyStr.c_str(), hasSaveStr.c_str());

        if (!request.HasMember("username") || !request.HasMember("isLegacy") || !request.HasMember("save")) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "Missing required fields in request");
            ctx->set_response_http_code(400);
            cb("{\"error\": \"Missing required fields\"}");
            return;
        }

        const std::string username = request["username"].GetString();
        const bool isLegacy = request["isLegacy"].GetBool();
        const std::string saveText = request["save"].GetString();

        try {
            Data::LandMessage save_data;
            if (!google::protobuf::TextFormat::ParseFromString(saveText, &save_data)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                    "Failed to parse text format for user: %s", username.c_str());
                ctx->set_response_http_code(400);
                cb("{\"error\": \"Invalid save data format\"}");
                return;
            }

            std::filesystem::path savePath = std::filesystem::current_path() / "towns";
            if (isLegacy) {
                savePath /= "mytown.pb";
            } else {
                savePath /= (username + ".pb");
            }

            if (std::filesystem::exists(savePath)) {
                std::filesystem::path backupPath = savePath;
                backupPath += ".backup";
                std::filesystem::copy_file(savePath, backupPath, std::filesystem::copy_options::overwrite_existing);
            }

            std::ofstream saveFile(savePath, std::ios::binary);
            if (!saveFile.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                    "Failed to open save file for writing: %s", savePath.string().c_str());
                ctx->set_response_http_code(500);
                cb("{\"error\": \"Failed to open save file for writing\"}");
                return;
            }

            if (!save_data.SerializeToOstream(&saveFile)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                    "Failed to serialize save data for user: %s", username.c_str());
                ctx->set_response_http_code(500);
                cb("{\"error\": \"Failed to write save data\"}");
                return;
            }

            saveFile.close();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, 
                "Successfully saved data for user: %s", username.c_str());

            ctx->set_response_http_code(200);
            cb("{\"success\": true}");
        } catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, 
                "Exception while saving: %s", e.what());
            ctx->set_response_http_code(500);
            cb(std::string(R"({"error": "Failed to save file: )") + e.what() + "\"}");
        }
    }

}
