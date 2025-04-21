#include <std_include.hpp>
#include "dashboard_config.hpp"
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

namespace tsto::dashboard {

    void DashboardConfig ::handle_update_backup_settings(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("success", false, doc.GetAllocator());
            doc.AddMember("error", "Unauthorized", doc.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string body = ctx->body().ToString();
        rapidjson::Document doc;
        doc.Parse(body.c_str());

        if (doc.HasParseError() || !doc.IsObject()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Invalid request body", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string backup_dir;
        int backup_interval_hours = 0;
        int backup_interval_seconds = 0;

        if (doc.HasMember("backupDirectory") && doc["backupDirectory"].IsString()) {
            backup_dir = doc["backupDirectory"].GetString();
        }

        if (doc.HasMember("backupIntervalHours") && doc["backupIntervalHours"].IsInt()) {
            backup_interval_hours = doc["backupIntervalHours"].GetInt();
        }

        if (doc.HasMember("backupIntervalSeconds") && doc["backupIntervalSeconds"].IsInt()) {
            backup_interval_seconds = doc["backupIntervalSeconds"].GetInt();
        }

        utils::configuration::WriteString("Backup", "BackupDirectory", backup_dir);
        utils::configuration::WriteInteger("Backup", "IntervalHours", backup_interval_hours);
        utils::configuration::WriteInteger("Backup", "IntervalSeconds", backup_interval_seconds);

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
            "Updated backup settings: Directory=%s, IntervalHours=%d, IntervalSeconds=%d",
            backup_dir.c_str(), backup_interval_hours, backup_interval_seconds);

        rapidjson::Document response;
        response.SetObject();
        response.AddMember("success", true, response.GetAllocator());
        
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        
        ctx->set_response_http_code(200);
        ctx->AddResponseHeader("Content-Type", "application/json");
        cb(buffer.GetString());
    }

    void DashboardConfig::handle_update_api_settings(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("success", false, doc.GetAllocator());
            doc.AddMember("error", "Unauthorized", doc.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string body = ctx->body().ToString();
        rapidjson::Document doc;
        doc.Parse(body.c_str());

        if (doc.HasParseError() || !doc.IsObject()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Invalid request body", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        bool api_enabled = false;
        std::string api_key;
        std::string team_name = "Bodnjenie";
        bool require_code = false;

        if (doc.HasMember("apiEnabled") && doc["apiEnabled"].IsBool()) {
            api_enabled = doc["apiEnabled"].GetBool();
        }

        if (doc.HasMember("apiKey") && doc["apiKey"].IsString()) {
            api_key = doc["apiKey"].GetString();
            if (api_key.find('%') != std::string::npos || api_key.empty()) {
                std::string current_key = utils::configuration::ReadString("TSTO_API", "ApiKey", "");
                if (current_key.find('%') == std::string::npos && !current_key.empty()) {
                    api_key = current_key;
                }
            }
        }

        if (doc.HasMember("teamName") && doc["teamName"].IsString()) {
            team_name = doc["teamName"].GetString();
            if (team_name.find('%') != std::string::npos || team_name.empty()) {
                team_name = "Bodnjenie";
            }
        }

        if (doc.HasMember("requireCode") && doc["requireCode"].IsBool()) {
            require_code = doc["requireCode"].GetBool();
        }

        utils::configuration::WriteBoolean("TSTO_API", "Enabled", api_enabled);
        utils::configuration::WriteString("TSTO_API", "ApiKey", api_key);
        utils::configuration::WriteString("TSTO_API", "TeamName", team_name);
        utils::configuration::WriteString("TSTO_API", "RequireCode", require_code ? "true" : "false");

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
            "Updated API settings: Enabled=%s, TeamName=%s, RequireCode=%s",
            api_enabled ? "true" : "false", team_name.c_str(), require_code ? "true" : "false");

        rapidjson::Document response;
        response.SetObject();
        response.AddMember("success", true, response.GetAllocator());
        
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        
        ctx->set_response_http_code(200);
        ctx->AddResponseHeader("Content-Type", "application/json");
        cb(buffer.GetString());
    }

    void DashboardConfig::handle_update_land_settings(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("success", false, doc.GetAllocator());
            doc.AddMember("error", "Unauthorized", doc.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string body = ctx->body().ToString();
        rapidjson::Document doc;
        doc.Parse(body.c_str());

        if (doc.HasParseError() || !doc.IsObject()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Invalid request body", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        bool use_legacy_mode = true;

        if (doc.HasMember("useLegacyMode") && doc["useLegacyMode"].IsBool()) {
            use_legacy_mode = doc["useLegacyMode"].GetBool();
        }

        utils::configuration::WriteBoolean("Land", "UseLegacyMode", use_legacy_mode);

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
            "Updated land settings: UseLegacyMode=%s",
            use_legacy_mode ? "true" : "false");

        rapidjson::Document response;
        response.SetObject();
        response.AddMember("success", true, response.GetAllocator());
        
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        
        ctx->set_response_http_code(200);
        ctx->AddResponseHeader("Content-Type", "application/json");
        cb(buffer.GetString());
    }

    void DashboardConfig::handle_update_security_settings(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("success", false, doc.GetAllocator());
            doc.AddMember("error", "Unauthorized", doc.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string body = ctx->body().ToString();
        rapidjson::Document doc;
        doc.Parse(body.c_str());

        if (doc.HasParseError() || !doc.IsObject()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Invalid request body", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        bool disable_anonymous_users = false;

        if (doc.HasMember("disableAnonymousUsers") && doc["disableAnonymousUsers"].IsBool()) {
            disable_anonymous_users = doc["disableAnonymousUsers"].GetBool();
        }

        utils::configuration::WriteBoolean("Security", "DisableAnonymousUsers", disable_anonymous_users);

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_INITIALIZER,
            "Updated security settings: DisableAnonymousUsers=%s",
            disable_anonymous_users ? "true" : "false");

        rapidjson::Document response;
        response.SetObject();
        response.AddMember("success", true, response.GetAllocator());
        
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        
        ctx->set_response_http_code(200);
        ctx->AddResponseHeader("Content-Type", "application/json");
        cb(buffer.GetString());
    }

    void DashboardConfig::handle_get_dashboard_config(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("success", false, doc.GetAllocator());
            doc.AddMember("error", "Unauthorized", doc.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        rapidjson::Document response;
        response.SetObject();
        auto& allocator = response.GetAllocator();
        
        response.AddMember("success", true, allocator);
        
        rapidjson::Value config(rapidjson::kObjectType);
        
        rapidjson::Value backup(rapidjson::kObjectType);
        backup.AddMember("backupDirectory", 
            rapidjson::Value(utils::configuration::ReadString("Backup", "BackupDirectory", "").c_str(), allocator), 
            allocator);
        backup.AddMember("backupIntervalHours", 
            utils::configuration::ReadInteger("Backup", "IntervalHours", 12), 
            allocator);
        backup.AddMember("backupIntervalSeconds", 
            utils::configuration::ReadInteger("Backup", "IntervalSeconds", 0), 
            allocator);
        config.AddMember("backup", backup, allocator);
        
        rapidjson::Value api(rapidjson::kObjectType);
        api.AddMember("apiEnabled", 
            utils::configuration::ReadBoolean("TSTO_API", "Enabled", false), 
            allocator);
            
        std::string apiKey = utils::configuration::ReadString("TSTO_API", "ApiKey", "");
        if (apiKey.find('%') != std::string::npos) {
            apiKey = "";      
        }
        api.AddMember("apiKey", 
            rapidjson::Value(apiKey.c_str(), allocator), 
            allocator);
            
        std::string teamName = utils::configuration::ReadString("TSTO_API", "TeamName", "Bodnjenie");
        if (teamName.find('%') != std::string::npos) {
            teamName = "Bodnjenie";      
        }
        api.AddMember("teamName", 
            rapidjson::Value(teamName.c_str(), allocator), 
            allocator);
            
        api.AddMember("requireCode", 
            utils::configuration::ReadString("TSTO_API", "RequireCode", "false") == "true", 
            allocator);
        config.AddMember("api", api, allocator);
        
        rapidjson::Value land(rapidjson::kObjectType);
        land.AddMember("useLegacyMode", 
            utils::configuration::ReadBoolean("Land", "UseLegacyMode", true), 
            allocator);
        config.AddMember("land", land, allocator);
        
        rapidjson::Value security(rapidjson::kObjectType);
        security.AddMember("disableAnonymousUsers", 
            utils::configuration::ReadBoolean("Security", "DisableAnonymousUsers", false), 
            allocator);
        config.AddMember("security", security, allocator);
        
        response.AddMember("config", config, allocator);
        
        response.AddMember("game_port", 
            utils::configuration::ReadUnsignedInteger("ServerConfig", "GamePort", 8080), 
            allocator);
        
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        response.Accept(writer);
        
        ctx->set_response_http_code(200);
        ctx->AddResponseHeader("Content-Type", "application/json");
        cb(buffer.GetString());
    }

    void DashboardConfig::handle_update_initial_donuts(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("success", false, doc.GetAllocator());
            doc.AddMember("error", "Unauthorized", doc.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        try {
            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("initialDonuts") || !doc["initialDonuts"].IsInt()) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "Invalid request body", response.GetAllocator());
                
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);
                
                ctx->set_response_http_code(400);
                ctx->AddResponseHeader("Content-Type", "application/json");
                cb(buffer.GetString());
                return;
            }

            int initial_donuts = doc["initialDonuts"].GetInt();
            utils::configuration::WriteString("Server", "InitialDonutAmount", std::to_string(initial_donuts));

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[DONUTS] Updated initial donuts amount to: %d", initial_donuts);

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", true, response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(200);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[DONUTS] Error updating initial donuts: %s", ex.what());
                
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Internal server error", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

}   
