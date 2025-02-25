#include <std_include.hpp>
#include "dashboard.hpp"
#include "configuration.hpp"
#include "debugging/serverlog.hpp"
#include <Windows.h>
#include <ShlObj.h>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

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
                    "Failed to restart server: {}", GetLastError());
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
                    "Dashboard template not found at {}", templatePath.string().c_str());
                cb("Error: Dashboard template file not found");
                return;
            }

            std::ifstream file(templatePath, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_INITIALIZER,
                    "Failed to open dashboard template: {}", templatePath.string().c_str());
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
                "Dashboard error: {}", ex.what());
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

}
