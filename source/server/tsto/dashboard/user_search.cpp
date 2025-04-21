#include <std_include.hpp>
#include "user_search.hpp"
#include "dashboard.hpp"
#include "debugging/serverlog.hpp"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>
#include <vector>
#include "tsto/database/database.hpp"
#include "headers/response_headers.hpp"

namespace tsto::dashboard {

    void UserSearch::handle_search_users(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Unauthorized: Admin access required", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string field = ctx->GetQuery("field");
        std::string term = ctx->GetQuery("term");

        if (field.empty() || term.empty()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Missing search parameters", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "[USER_SEARCH] Searching for users with %s='%s'", field.c_str(), term.c_str());

        try {
            std::vector<std::string> all_emails;
            if (!database::Database::get_instance().get_all_emails(all_emails)) {
                throw std::runtime_error("Failed to get emails from database");
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();
            
            rapidjson::Value users_array(rapidjson::kArrayType);

            for (const auto& email : all_emails) {
                bool match = false;

                if (field == "email" && email.find(term) != std::string::npos) {
                    match = true;
                }

                std::string user_id, access_token, device_id;
                std::string access_code, user_cred, display_name, town_name;
                std::string land_save_path, android_id, vendor_id, client_ip, combined_id;
                std::string mayhem_id, advertising_id, platform_id;
                std::string manufacturer, model, anon_uid, land_token, session_key, lnglv_token, as_identifier;
                
                database::Database::get_instance().get_user_id(email, user_id);
                database::Database::get_instance().get_access_token(email, access_token);
                database::Database::get_instance().get_device_id(email, device_id);
                database::Database::get_instance().get_display_name(email, display_name);
                database::Database::get_instance().get_town_name(email, town_name);
                database::Database::get_instance().get_land_save_path(email, land_save_path);
                database::Database::get_instance().get_android_id(email, android_id);
                database::Database::get_instance().get_vendor_id(email, vendor_id);
                database::Database::get_instance().get_client_ip(email, client_ip);
                database::Database::get_instance().get_combined_id(email, combined_id);
                database::Database::get_instance().get_user_cred(email, user_cred);
                database::Database::get_instance().get_access_code(email, access_code);
                database::Database::get_instance().get_mayhem_id(email, mayhem_id);
                database::Database::get_instance().get_advertising_id(email, advertising_id);
                database::Database::get_instance().get_platform_id(email, platform_id);
                database::Database::get_instance().get_manufacturer(email, manufacturer);
                database::Database::get_instance().get_model(email, model);
                database::Database::get_instance().get_anon_uid(email, anon_uid);
                database::Database::get_instance().get_land_token(email, land_token);
                database::Database::get_instance().get_session_key(email, session_key);
                database::Database::get_instance().get_lnglv_token(email, lnglv_token);
                database::Database::get_instance().get_as_identifier(email, as_identifier);

                if (field == "user_id" && user_id.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "display_name" && display_name.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "town_name" && town_name.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "device_id" && device_id.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "android_id" && android_id.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "vendor_id" && vendor_id.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "client_ip" && client_ip.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "combined_id" && combined_id.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "access_token" && access_token.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "access_code" && access_code.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "user_cred" && user_cred.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "land_save_path" && land_save_path.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "mayhem_id" && mayhem_id.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "advertising_id" && advertising_id.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "platform_id" && platform_id.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "manufacturer" && manufacturer.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "model" && model.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "anon_uid" && anon_uid.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "land_token" && land_token.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "session_key" && session_key.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "lnglv_token" && lnglv_token.find(term) != std::string::npos) {
                    match = true;
                } else if (field == "as_identifier" && as_identifier.find(term) != std::string::npos) {
                    match = true;
                }

                if (match) {
                    rapidjson::Value user_obj(rapidjson::kObjectType);
                    user_obj.AddMember("email", rapidjson::Value(email.c_str(), allocator), allocator);
                    user_obj.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                    user_obj.AddMember("access_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
                    user_obj.AddMember("device_id", rapidjson::Value(device_id.c_str(), allocator), allocator);
                    user_obj.AddMember("access_code", rapidjson::Value(access_code.c_str(), allocator), allocator);
                    user_obj.AddMember("user_cred", rapidjson::Value(user_cred.c_str(), allocator), allocator);
                    user_obj.AddMember("display_name", rapidjson::Value(display_name.c_str(), allocator), allocator);
                    user_obj.AddMember("town_name", rapidjson::Value(town_name.c_str(), allocator), allocator);
                    user_obj.AddMember("land_save_path", rapidjson::Value(land_save_path.c_str(), allocator), allocator);
                    user_obj.AddMember("android_id", rapidjson::Value(android_id.c_str(), allocator), allocator);
                    user_obj.AddMember("vendor_id", rapidjson::Value(vendor_id.c_str(), allocator), allocator);
                    user_obj.AddMember("client_ip", rapidjson::Value(client_ip.c_str(), allocator), allocator);
                    user_obj.AddMember("combined_id", rapidjson::Value(combined_id.c_str(), allocator), allocator);
                    user_obj.AddMember("advertising_id", rapidjson::Value(advertising_id.c_str(), allocator), allocator);
                    user_obj.AddMember("platform_id", rapidjson::Value(platform_id.c_str(), allocator), allocator);
                    user_obj.AddMember("manufacturer", rapidjson::Value(manufacturer.c_str(), allocator), allocator);
                    user_obj.AddMember("model", rapidjson::Value(model.c_str(), allocator), allocator);
                    user_obj.AddMember("mayhem_id", rapidjson::Value(mayhem_id.c_str(), allocator), allocator);
                    user_obj.AddMember("anon_uid", rapidjson::Value(anon_uid.c_str(), allocator), allocator);
                    user_obj.AddMember("land_token", rapidjson::Value(land_token.c_str(), allocator), allocator);
                    user_obj.AddMember("session_key", rapidjson::Value(session_key.c_str(), allocator), allocator);
                    user_obj.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);
                    user_obj.AddMember("as_identifier", rapidjson::Value(as_identifier.c_str(), allocator), allocator);
                    
                    users_array.PushBack(user_obj, allocator);
                }
            }

            response.AddMember("success", true, allocator);
            response.AddMember("users", users_array, allocator);
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(200);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[USER_SEARCH] Error searching users: %s", ex.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Failed to search users", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

    void UserSearch::handle_get_user(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Unauthorized: Admin access required", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string email = ctx->GetQuery("email");

        if (email.empty()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Missing email parameter", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "[USER_SEARCH] Getting details for user with email='%s'", email.c_str());

        try {
            std::string user_id, access_token, device_id;
            std::string access_code, user_cred, display_name, town_name;
            std::string land_save_path, android_id, vendor_id, client_ip, combined_id;
            std::string mayhem_id = "";
            std::string advertising_id, platform_id;
            std::string manufacturer = "";
            std::string model = "";
            std::string anon_uid = "";
            std::string land_token = "";
            std::string session_key = "";
            std::string lnglv_token = "";
            std::string as_identifier = "";
            
            database::Database::get_instance().get_user_id(email, user_id);
            database::Database::get_instance().get_access_token(email, access_token);
            database::Database::get_instance().get_device_id(email, device_id);
            database::Database::get_instance().get_display_name(email, display_name);
            database::Database::get_instance().get_town_name(email, town_name);
            database::Database::get_instance().get_land_save_path(email, land_save_path);
            database::Database::get_instance().get_android_id(email, android_id);
            database::Database::get_instance().get_vendor_id(email, vendor_id);
            database::Database::get_instance().get_client_ip(email, client_ip);
            database::Database::get_instance().get_combined_id(email, combined_id);
            database::Database::get_instance().get_user_cred(email, user_cred);
            database::Database::get_instance().get_access_code(email, access_code);
            database::Database::get_instance().get_mayhem_id(email, mayhem_id);
            database::Database::get_instance().get_advertising_id(email, advertising_id);
            database::Database::get_instance().get_platform_id(email, platform_id);
            database::Database::get_instance().get_manufacturer(email, manufacturer);
            database::Database::get_instance().get_model(email, model);
            database::Database::get_instance().get_anon_uid(email, anon_uid);
            database::Database::get_instance().get_land_token(email, land_token);
            database::Database::get_instance().get_session_key(email, session_key);
            database::Database::get_instance().get_lnglv_token(email, lnglv_token);
            database::Database::get_instance().get_as_identifier(email, as_identifier);

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();
            
            rapidjson::Value user_obj(rapidjson::kObjectType);
            user_obj.AddMember("email", rapidjson::Value(email.c_str(), allocator), allocator);
            user_obj.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            user_obj.AddMember("access_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
            user_obj.AddMember("device_id", rapidjson::Value(device_id.c_str(), allocator), allocator);
            user_obj.AddMember("access_code", rapidjson::Value(access_code.c_str(), allocator), allocator);
            user_obj.AddMember("user_cred", rapidjson::Value(user_cred.c_str(), allocator), allocator);
            user_obj.AddMember("display_name", rapidjson::Value(display_name.c_str(), allocator), allocator);
            user_obj.AddMember("town_name", rapidjson::Value(town_name.c_str(), allocator), allocator);
            user_obj.AddMember("land_save_path", rapidjson::Value(land_save_path.c_str(), allocator), allocator);
            user_obj.AddMember("android_id", rapidjson::Value(android_id.c_str(), allocator), allocator);
            user_obj.AddMember("vendor_id", rapidjson::Value(vendor_id.c_str(), allocator), allocator);
            user_obj.AddMember("client_ip", rapidjson::Value(client_ip.c_str(), allocator), allocator);
            user_obj.AddMember("combined_id", rapidjson::Value(combined_id.c_str(), allocator), allocator);
            user_obj.AddMember("manufacturer", rapidjson::Value(manufacturer.c_str(), allocator), allocator);
            user_obj.AddMember("model", rapidjson::Value(model.c_str(), allocator), allocator);
            user_obj.AddMember("mayhem_id", rapidjson::Value(mayhem_id.c_str(), allocator), allocator);
            user_obj.AddMember("anon_uid", rapidjson::Value(anon_uid.c_str(), allocator), allocator);
            user_obj.AddMember("land_token", rapidjson::Value(land_token.c_str(), allocator), allocator);
            user_obj.AddMember("session_key", rapidjson::Value(session_key.c_str(), allocator), allocator);
            user_obj.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);
            user_obj.AddMember("as_identifier", rapidjson::Value(as_identifier.c_str(), allocator), allocator);
            
            response.AddMember("success", true, allocator);
            response.AddMember("user", user_obj, allocator);
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(200);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[USER_SEARCH] Error getting user details: %s", ex.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Failed to get user details", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

    void UserSearch::handle_update_user(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Unauthorized: Admin access required", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string body = ctx->body().ToString();
        rapidjson::Document doc;
        doc.Parse(body.c_str());

        if (doc.HasParseError() || !doc.IsObject() || !doc.HasMember("email") || !doc["email"].IsString()) {
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

        std::string email = doc["email"].GetString();
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "[USER_SEARCH] Updating user with email='%s'", email.c_str());

        try {
            if (doc.HasMember("display_name") && doc["display_name"].IsString()) {
                std::string display_name = doc["display_name"].GetString();
                database::Database::get_instance().update_display_name(email, display_name);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated display_name for %s to '%s'", email.c_str(), display_name.c_str());
            }

            if (doc.HasMember("town_name") && doc["town_name"].IsString()) {
                std::string town_name = doc["town_name"].GetString();
                database::Database::get_instance().update_town_name(email, town_name);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated town_name for %s to '%s'", email.c_str(), town_name.c_str());
            }

            if (doc.HasMember("access_token") && doc["access_token"].IsString()) {
                std::string access_token = doc["access_token"].GetString();
                database::Database::get_instance().update_access_token(email, access_token);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated access_token for %s to '%s'", email.c_str(), access_token.c_str());
            }

            if (doc.HasMember("access_code") && doc["access_code"].IsString()) {
                std::string access_code = doc["access_code"].GetString();
                database::Database::get_instance().update_access_code(email, access_code);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated access_code for %s to '%s'", email.c_str(), access_code.c_str());
            }

            if (doc.HasMember("user_cred") && doc["user_cred"].IsString()) {
                std::string user_cred = doc["user_cred"].GetString();
                database::Database::get_instance().update_user_cred(email, user_cred);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated user_cred for %s to '%s'", email.c_str(), user_cred.c_str());
            }

            if (doc.HasMember("land_save_path") && doc["land_save_path"].IsString()) {
                std::string land_save_path = doc["land_save_path"].GetString();
                database::Database::get_instance().update_land_save_path(email, land_save_path);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated land_save_path for %s to '%s'", email.c_str(), land_save_path.c_str());
            }

            if (doc.HasMember("android_id") && doc["android_id"].IsString()) {
                std::string android_id = doc["android_id"].GetString();
                database::Database::get_instance().update_android_id(email, android_id);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated android_id for %s to '%s'", email.c_str(), android_id.c_str());
            }

            if (doc.HasMember("vendor_id") && doc["vendor_id"].IsString()) {
                std::string vendor_id = doc["vendor_id"].GetString();
                database::Database::get_instance().update_vendor_id(email, vendor_id);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated vendor_id for %s to '%s'", email.c_str(), vendor_id.c_str());
            }

            if (doc.HasMember("client_ip") && doc["client_ip"].IsString()) {
                std::string client_ip = doc["client_ip"].GetString();
                database::Database::get_instance().update_client_ip(email, client_ip);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated client_ip for %s to '%s'", email.c_str(), client_ip.c_str());
            }

            if (doc.HasMember("combined_id") && doc["combined_id"].IsString()) {
                std::string combined_id = doc["combined_id"].GetString();
                database::Database::get_instance().update_combined_id(email, combined_id);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated combined_id for %s to '%s'", email.c_str(), combined_id.c_str());
            }

            if (doc.HasMember("mayhem_id")) {
                std::string mayhem_id;
                if (doc["mayhem_id"].IsInt64()) {
                    mayhem_id = std::to_string(doc["mayhem_id"].GetInt64());
                } else if (doc["mayhem_id"].IsString()) {
                    mayhem_id = doc["mayhem_id"].GetString();
                }
                
                if (!mayhem_id.empty()) {
                    database::Database::get_instance().update_mayhem_id(email, mayhem_id);
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "[USER_SEARCH] Updated mayhem_id for %s to '%s'", email.c_str(), mayhem_id.c_str());
                }
            }

            if (doc.HasMember("manufacturer") && doc["manufacturer"].IsString()) {
                std::string manufacturer = doc["manufacturer"].GetString();
                database::Database::get_instance().update_manufacturer(email, manufacturer);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated manufacturer for %s to '%s'", email.c_str(), manufacturer.c_str());
            }

            if (doc.HasMember("model") && doc["model"].IsString()) {
                std::string model = doc["model"].GetString();
                database::Database::get_instance().update_model(email, model);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated model for %s to '%s'", email.c_str(), model.c_str());
            }

            if (doc.HasMember("as_identifier") && doc["as_identifier"].IsString()) {
                std::string as_identifier = doc["as_identifier"].GetString();
                database::Database::get_instance().update_as_identifier(email, as_identifier);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                    "[USER_SEARCH] Updated as_identifier for %s to '%s'", email.c_str(), as_identifier.c_str());
            }

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
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[USER_SEARCH] Error updating user: %s", ex.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Failed to update user", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

    void UserSearch::handle_delete_user(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        const std::string method = ctx->GetMethod();
        if (method != "DELETE") {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Method not allowed. Use DELETE method for this endpoint", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(405);    
            ctx->AddResponseHeader("Content-Type", "application/json");
            ctx->AddResponseHeader("Allow", "DELETE");
            cb(buffer.GetString());
            return;
        }
        
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Unauthorized: Admin access required", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string email = ctx->GetQuery("email");

        if (email.empty()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Missing email parameter", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "[USER_SEARCH] Deleting user with email='%s'", email.c_str());

        try {
            if (!database::Database::get_instance().delete_user(email)) {
                throw std::runtime_error("Failed to delete user from database");
            }

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
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[USER_SEARCH] Error deleting user: %s", ex.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Failed to delete user", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

    void UserSearch::handle_get_all_users(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        if (!Dashboard::is_admin(ctx)) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Unauthorized: Admin access required", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(403);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "[USER_SEARCH] Getting all users");

        try {
            std::vector<std::string> all_emails;
            if (!database::Database::get_instance().get_all_emails(all_emails)) {
                throw std::runtime_error("Failed to get emails from database");
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();
            
            rapidjson::Value users_array(rapidjson::kArrayType);

            for (const auto& email : all_emails) {
                std::string user_id, access_token, device_id;
                std::string access_code, user_cred, display_name, town_name;
                std::string land_save_path, android_id, vendor_id, client_ip, combined_id;
                std::string mayhem_id, advertising_id, platform_id;
                std::string manufacturer, model, anon_uid, land_token, session_key, lnglv_token, as_identifier;
                
                database::Database::get_instance().get_user_id(email, user_id);
                database::Database::get_instance().get_access_token(email, access_token);
                database::Database::get_instance().get_device_id(email, device_id);
                database::Database::get_instance().get_display_name(email, display_name);
                database::Database::get_instance().get_town_name(email, town_name);
                database::Database::get_instance().get_land_save_path(email, land_save_path);
                database::Database::get_instance().get_android_id(email, android_id);
                database::Database::get_instance().get_vendor_id(email, vendor_id);
                database::Database::get_instance().get_client_ip(email, client_ip);
                database::Database::get_instance().get_combined_id(email, combined_id);
                database::Database::get_instance().get_user_cred(email, user_cred);
                database::Database::get_instance().get_access_code(email, access_code);
                database::Database::get_instance().get_mayhem_id(email, mayhem_id);
                database::Database::get_instance().get_advertising_id(email, advertising_id);
                database::Database::get_instance().get_platform_id(email, platform_id);
                database::Database::get_instance().get_manufacturer(email, manufacturer);
                database::Database::get_instance().get_model(email, model);
                database::Database::get_instance().get_anon_uid(email, anon_uid);
                database::Database::get_instance().get_land_token(email, land_token);
                database::Database::get_instance().get_session_key(email, session_key);
                database::Database::get_instance().get_lnglv_token(email, lnglv_token);
                database::Database::get_instance().get_as_identifier(email, as_identifier);

                rapidjson::Value user_obj(rapidjson::kObjectType);
                user_obj.AddMember("email", rapidjson::Value(email.c_str(), allocator), allocator);
                user_obj.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                user_obj.AddMember("access_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
                user_obj.AddMember("device_id", rapidjson::Value(device_id.c_str(), allocator), allocator);
                user_obj.AddMember("access_code", rapidjson::Value(access_code.c_str(), allocator), allocator);
                user_obj.AddMember("user_cred", rapidjson::Value(user_cred.c_str(), allocator), allocator);
                user_obj.AddMember("display_name", rapidjson::Value(display_name.c_str(), allocator), allocator);
                user_obj.AddMember("town_name", rapidjson::Value(town_name.c_str(), allocator), allocator);
                user_obj.AddMember("land_save_path", rapidjson::Value(land_save_path.c_str(), allocator), allocator);
                user_obj.AddMember("android_id", rapidjson::Value(android_id.c_str(), allocator), allocator);
                user_obj.AddMember("vendor_id", rapidjson::Value(vendor_id.c_str(), allocator), allocator);
                user_obj.AddMember("client_ip", rapidjson::Value(client_ip.c_str(), allocator), allocator);
                user_obj.AddMember("combined_id", rapidjson::Value(combined_id.c_str(), allocator), allocator);
                user_obj.AddMember("advertising_id", rapidjson::Value(advertising_id.c_str(), allocator), allocator);
                user_obj.AddMember("platform_id", rapidjson::Value(platform_id.c_str(), allocator), allocator);
                user_obj.AddMember("manufacturer", rapidjson::Value(manufacturer.c_str(), allocator), allocator);
                user_obj.AddMember("model", rapidjson::Value(model.c_str(), allocator), allocator);
                user_obj.AddMember("mayhem_id", rapidjson::Value(mayhem_id.c_str(), allocator), allocator);
                user_obj.AddMember("anon_uid", rapidjson::Value(anon_uid.c_str(), allocator), allocator);
                user_obj.AddMember("land_token", rapidjson::Value(land_token.c_str(), allocator), allocator);
                user_obj.AddMember("session_key", rapidjson::Value(session_key.c_str(), allocator), allocator);
                user_obj.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);
                user_obj.AddMember("as_identifier", rapidjson::Value(as_identifier.c_str(), allocator), allocator);
                
                users_array.PushBack(user_obj, allocator);
            }

            response.AddMember("success", true, allocator);
            response.AddMember("users", users_array, allocator);
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(200);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[USER_SEARCH] Error getting all users: %s", ex.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Failed to get all users", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

}   
