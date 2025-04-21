#include <std_include.hpp>
#include "public_user_search.hpp"
#include "dashboard.hpp"
#include "debugging/serverlog.hpp"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <string>
#include <vector>
#include "tsto/database/public_users_db.hpp"
#include "headers/response_headers.hpp"

namespace tsto::dashboard {

    void PublicUserSearch::handle_search_public_users(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
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
            "[PUBLIC_USER_SEARCH] Searching for users with %s='%s'", field.c_str(), term.c_str());

        try {
            std::vector<std::string> matching_emails;
            if (!tsto::database::PublicUsersDB::get_instance().search_users(field, term, matching_emails)) {
                throw std::runtime_error("Failed to search users in database");
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();
            
            rapidjson::Value users_array(rapidjson::kArrayType);

            for (const auto& email : matching_emails) {
                std::string display_name;
                bool is_verified;
                int64_t registration_date, last_login;
                
                if (tsto::database::PublicUsersDB::get_instance().get_user_info(email, display_name, is_verified, registration_date, last_login)) {
                    rapidjson::Value user_obj(rapidjson::kObjectType);
                    
                    rapidjson::Value email_val(email.c_str(), allocator);
                    user_obj.AddMember("email", email_val, allocator);
                    
                    rapidjson::Value display_name_val(display_name.c_str(), allocator);
                    user_obj.AddMember("display_name", display_name_val, allocator);
                    
                    user_obj.AddMember("is_verified", is_verified, allocator);
                    user_obj.AddMember("registration_date", registration_date, allocator);
                    user_obj.AddMember("last_login", last_login, allocator);
                    
                    users_array.PushBack(user_obj, allocator);
                }
            }
            
            response.AddMember("success", true, allocator);
            response.AddMember("users", users_array, allocator);
            response.AddMember("count", static_cast<int>(users_array.Size()), allocator);
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(200);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[PUBLIC_USER_SEARCH] Error searching users: %s", e.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            
            rapidjson::Value error_val(e.what(), response.GetAllocator());
            response.AddMember("error", error_val, response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

    void PublicUserSearch::handle_get_public_user(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
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
            "[PUBLIC_USER_SEARCH] Getting user details for %s", email.c_str());

        try {
            if (!tsto::database::PublicUsersDB::get_instance().user_exists(email)) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "User not found", response.GetAllocator());
                
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);
                
                ctx->set_response_http_code(404);
                ctx->AddResponseHeader("Content-Type", "application/json");
                cb(buffer.GetString());
                return;
            }

            std::string display_name;
            bool is_verified;
            int64_t registration_date, last_login;
            
            if (!tsto::database::PublicUsersDB::get_instance().get_user_info(email, display_name, is_verified, registration_date, last_login)) {
                throw std::runtime_error("Failed to get user info from database");
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();
            
            response.AddMember("success", true, allocator);
            
            rapidjson::Value user_obj(rapidjson::kObjectType);
            
            rapidjson::Value email_val(email.c_str(), allocator);
            user_obj.AddMember("email", email_val, allocator);
            
            rapidjson::Value display_name_val(display_name.c_str(), allocator);
            user_obj.AddMember("display_name", display_name_val, allocator);
            
            user_obj.AddMember("is_verified", is_verified, allocator);
            user_obj.AddMember("registration_date", registration_date, allocator);
            user_obj.AddMember("last_login", last_login, allocator);
            
            response.AddMember("user", user_obj, allocator);
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(200);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[PUBLIC_USER_SEARCH] Error getting user details: %s", e.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            
            rapidjson::Value error_val(e.what(), response.GetAllocator());
            response.AddMember("error", error_val, response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

    void PublicUserSearch::handle_update_public_user(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
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
        if (body.empty()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Missing request body", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        rapidjson::Document request_data;
        if (request_data.Parse(body.c_str()).HasParseError()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Invalid JSON in request body", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        if (!request_data.HasMember("email") || !request_data["email"].IsString()) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "Missing or invalid email in request", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(400);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        std::string email = request_data["email"].GetString();
        
        if (!tsto::database::PublicUsersDB::get_instance().user_exists(email)) {
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            response.AddMember("error", "User not found", response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(404);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
            return;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
            "[PUBLIC_USER_SEARCH] Updating user %s", email.c_str());

        try {
            bool updated = false;
            
            if (request_data.HasMember("display_name") && request_data["display_name"].IsString()) {
                std::string display_name = request_data["display_name"].GetString();
                
                if (tsto::database::PublicUsersDB::get_instance().update_display_name(email, display_name)) {
                    updated = true;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "[PUBLIC_USER_SEARCH] Updated display name for %s to %s", email.c_str(), display_name.c_str());
                }
            }
            
            if (request_data.HasMember("is_verified") && request_data["is_verified"].IsBool()) {
                bool is_verified = request_data["is_verified"].GetBool();
                
                if (tsto::database::PublicUsersDB::get_instance().update_verification_status(email, is_verified)) {
                    updated = true;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_DATABASE,
                        "[PUBLIC_USER_SEARCH] Updated verification status for %s to %s", 
                        email.c_str(), is_verified ? "verified" : "unverified");
                }
            }

            if (!updated) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "No fields were updated", response.GetAllocator());
                
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);
                
                ctx->set_response_http_code(400);
                ctx->AddResponseHeader("Content-Type", "application/json");
                cb(buffer.GetString());
                return;
            }

            std::string display_name;
            bool is_verified;
            int64_t registration_date, last_login;
            
            if (!tsto::database::PublicUsersDB::get_instance().get_user_info(email, display_name, is_verified, registration_date, last_login)) {
                throw std::runtime_error("Failed to get updated user info from database");
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();
            
            response.AddMember("success", true, allocator);
            
            rapidjson::Value user_obj(rapidjson::kObjectType);
            
            rapidjson::Value email_val(email.c_str(), allocator);
            user_obj.AddMember("email", email_val, allocator);
            
            rapidjson::Value display_name_val(display_name.c_str(), allocator);
            user_obj.AddMember("display_name", display_name_val, allocator);
            
            user_obj.AddMember("is_verified", is_verified, allocator);
            user_obj.AddMember("registration_date", registration_date, allocator);
            user_obj.AddMember("last_login", last_login, allocator);
            
            response.AddMember("user", user_obj, allocator);
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(200);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[PUBLIC_USER_SEARCH] Error updating user: %s", e.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            
            rapidjson::Value error_val(e.what(), response.GetAllocator());
            response.AddMember("error", error_val, response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

    void PublicUserSearch::handle_delete_public_user(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
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
            "[PUBLIC_USER_SEARCH] Deleting user %s", email.c_str());

        try {
            if (!tsto::database::PublicUsersDB::get_instance().user_exists(email)) {
                rapidjson::Document response;
                response.SetObject();
                response.AddMember("success", false, response.GetAllocator());
                response.AddMember("error", "User not found", response.GetAllocator());
                
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                response.Accept(writer);
                
                ctx->set_response_http_code(404);
                ctx->AddResponseHeader("Content-Type", "application/json");
                cb(buffer.GetString());
                return;
            }

            if (!tsto::database::PublicUsersDB::get_instance().delete_user(email)) {
                throw std::runtime_error("Failed to delete user from database");
            }

            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", true, response.GetAllocator());
            
            rapidjson::Value message_val("User deleted successfully", response.GetAllocator());
            response.AddMember("message", message_val, response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(200);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[PUBLIC_USER_SEARCH] Error deleting user: %s", e.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            
            rapidjson::Value error_val(e.what(), response.GetAllocator());
            response.AddMember("error", error_val, response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

    void PublicUserSearch::handle_get_all_public_users(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
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
            "[PUBLIC_USER_SEARCH] Getting all public users");

        try {
            std::vector<std::string> all_emails;
            if (!tsto::database::PublicUsersDB::get_instance().get_all_emails(all_emails)) {
                throw std::runtime_error("Failed to get emails from database");
            }

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();
            
            rapidjson::Value users_array(rapidjson::kArrayType);

            for (const auto& email : all_emails) {
                std::string display_name;
                bool is_verified;
                int64_t registration_date, last_login;
                
                if (tsto::database::PublicUsersDB::get_instance().get_user_info(email, display_name, is_verified, registration_date, last_login)) {
                    rapidjson::Value user_obj(rapidjson::kObjectType);
                    
                    rapidjson::Value email_val(email.c_str(), allocator);
                    user_obj.AddMember("email", email_val, allocator);
                    
                    rapidjson::Value display_name_val(display_name.c_str(), allocator);
                    user_obj.AddMember("display_name", display_name_val, allocator);
                    
                    user_obj.AddMember("is_verified", is_verified, allocator);
                    user_obj.AddMember("registration_date", registration_date, allocator);
                    user_obj.AddMember("last_login", last_login, allocator);
                    
                    users_array.PushBack(user_obj, allocator);
                }
            }
            
            response.AddMember("success", true, allocator);
            response.AddMember("users", users_array, allocator);
            response.AddMember("count", static_cast<int>(users_array.Size()), allocator);
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(200);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_DATABASE,
                "[PUBLIC_USER_SEARCH] Error getting all users: %s", e.what());
            
            rapidjson::Document response;
            response.SetObject();
            response.AddMember("success", false, response.GetAllocator());
            
            rapidjson::Value error_val(e.what(), response.GetAllocator());
            response.AddMember("error", error_val, response.GetAllocator());
            
            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);
            
            ctx->set_response_http_code(500);
            ctx->AddResponseHeader("Content-Type", "application/json");
            cb(buffer.GetString());
        }
    }

}   
