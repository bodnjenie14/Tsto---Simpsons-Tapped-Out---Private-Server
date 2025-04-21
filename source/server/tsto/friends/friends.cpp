#include <std_include.hpp>
#include "friends.hpp"
#include "tsto/database/database.hpp"
#include "tsto/includes/helpers.hpp"
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
#include <configuration.hpp>
#include <io.hpp>
#include "debugging/serverlog.hpp"
#include "tsto/includes/session.hpp"

#include <vector>
#include <tuple>
#include <string>

namespace tsto::friends {

    //void Friends::handle_get_inbound_invites(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
    //    const evpp::http::HTTPSendResponseCallback& cb) {
    //    try {
    //        logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_FRIENDS,
    //            "[FRIENDS] Inbound invites request from %s: %s", ctx->remote_ip().data(), ctx->uri().data());

    //        std::string uri = ctx->uri();
    //        std::string user_id;

    //        // Extract user ID from URL: /friends/2/users/{userId}/invitations/inbound
    //        std::regex pattern(R"(/friends/2/users/(\d+)/invitations/inbound)");
    //        std::smatch matches;
    //        if (std::regex_search(uri, matches, pattern) && matches.size() > 1) {
    //            user_id = matches[1].str();
    //        }

    //        if (user_id.empty()) {
    //            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FRIENDS,
    //                "[FRIENDS] Invalid user ID in URL: %s", uri.c_str());
    //            ctx->set_response_http_code(400);
    //            cb("{\"message\":\"Invalid user ID\"}");
    //            return;
    //        }

    //        // Validate auth token
    //        std::string access_token = helpers::extract_access_token(ctx, logger::LOG_LABEL_FRIENDS, "[FRIENDS]");
    //        if (access_token.empty()) {
    //            ctx->set_response_http_code(401);
    //            cb("{\"message\":\"Missing or invalid access token\"}");
    //            return;
    //        }

    //        auto& db = tsto::database::Database::get_instance();
    //        std::string email;
    //        std::string stored_user_id;

    //        if (!db.get_email_by_token(access_token, email) || !db.get_user_id(email, stored_user_id)) {
    //            ctx->set_response_http_code(401);
    //            cb("{\"message\":\"Invalid access token\"}");
    //            return;
    //        }

    //        // Get friend requests from database
    //        std::vector<std::tuple<std::string, std::string, std::string, int64_t>> requests;
    //        if (!db.get_friend_requests(user_id, requests)) {
    //            ctx->set_response_http_code(500);
    //            cb("{\"message\":\"Failed to get friend requests\"}");
    //            return;
    //        }

    //        // Build response JSON
    //        rapidjson::Document doc;
    //        doc.SetObject();
    //        rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    //        rapidjson::Value entries(rapidjson::kArrayType);
    //        for (const auto& [friend_id, display_name, nickname, timestamp] : requests) {
    //            rapidjson::Value entry(rapidjson::kObjectType);
    //            entry.AddMember("timestamp", timestamp, allocator);
    //            entry.AddMember("userId", std::stoll(friend_id), allocator);
    //            entry.AddMember("dateTime", helpers::format_iso8601(timestamp), allocator);
    //            
    //            rapidjson::Value invite_tags(rapidjson::kObjectType);
    //            invite_tags.AddMember("invite_surface", "unknown", allocator);
    //            entry.AddMember("inviteTags", invite_tags, allocator);
    //            
    //            entry.AddMember("userType", "NUCLEUS_USER", allocator);
    //            entry.AddMember("displayName", rapidjson::Value(display_name.c_str(), allocator), allocator);
    //            entry.AddMember("personaId", std::stoll(friend_id), allocator);
    //            entry.AddMember("nickName", rapidjson::Value(nickname.c_str(), allocator), allocator);
    //            
    //            entries.PushBack(entry, allocator);
    //        }
    //        doc.AddMember("entries", entries, allocator);

    //        rapidjson::Value paging(rapidjson::kObjectType);
    //        paging.AddMember("size", entries.Size(), allocator);
    //        paging.AddMember("offset", 0, allocator);
    //        paging.AddMember("totalSize", entries.Size(), allocator);
    //        doc.AddMember("pagingInfo", paging, allocator);

    //        rapidjson::StringBuffer buffer;
    //        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    //        doc.Accept(writer);

    //        ctx->set_response_http_code(200);
    //        cb(buffer.GetString());

    //    } catch (const std::exception& e) {
    //        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FRIENDS,
    //            "[FRIENDS] Error handling inbound invites: %s", e.what());
    //        ctx->set_response_http_code(500);
    //        cb("{\"message\":\"Internal server error\"}");
    //    }
    //}

    //void Friends::handle_get_outbound_invites(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
    //    const evpp::http::HTTPSendResponseCallback& cb) {
    //    try {
    //        logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_FRIENDS,
    //            "[FRIENDS] Outbound invites request from %s: %s", ctx->remote_ip().data(), ctx->uri().data());

    //        std::string uri = ctx->uri();
    //        std::string user_id;

    //        // Extract user ID from URL: /friends/2/users/{userId}/invitations/outbound
    //        std::regex pattern(R"(/friends/2/users/(\d+)/invitations/outbound)");
    //        std::smatch matches;
    //        if (std::regex_search(uri, matches, pattern) && matches.size() > 1) {
    //            user_id = matches[1].str();
    //        }

    //        if (user_id.empty()) {
    //            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FRIENDS,
    //                "[FRIENDS] Invalid user ID in URL: %s", uri.c_str());
    //            ctx->set_response_http_code(400);
    //            cb("{\"message\":\"Invalid user ID\"}");
    //            return;
    //        }

    //        // Validate auth token
    //        std::string access_token = helpers::extract_access_token(ctx, logger::LOG_LABEL_FRIENDS, "[FRIENDS]");
    //        if (access_token.empty()) {
    //            ctx->set_response_http_code(401);
    //            cb("{\"message\":\"Missing or invalid access token\"}");
    //            return;
    //        }

    //        auto& db = tsto::database::Database::get_instance();
    //        std::string email;
    //        std::string stored_user_id;

    //        if (!db.get_email_by_token(access_token, email) || !db.get_user_id(email, stored_user_id)) {
    //            ctx->set_response_http_code(401);
    //            cb("{\"message\":\"Invalid access token\"}");
    //            return;
    //        }

    //        // Get outbound friend requests from database
    //        std::vector<std::tuple<std::string, std::string, std::string, int64_t>> requests;
    //        if (!db.get_outbound_friend_requests(user_id, requests)) {
    //            ctx->set_response_http_code(500);
    //            cb("{\"message\":\"Failed to get outbound friend requests\"}");
    //            return;
    //        }

    //        // Build response JSON
    //        rapidjson::Document doc;
    //        doc.SetObject();
    //        rapidjson::Document::AllocatorType& allocator = doc.GetAllocator();

    //        rapidjson::Value entries(rapidjson::kArrayType);
    //        for (const auto& [friend_id, display_name, nickname, timestamp] : requests) {
    //            rapidjson::Value entry(rapidjson::kObjectType);
    //            entry.AddMember("timestamp", timestamp, allocator);
    //            entry.AddMember("userId", std::stoll(friend_id), allocator);
    //            entry.AddMember("dateTime", helpers::format_iso8601(timestamp), allocator);
    //            
    //            rapidjson::Value invite_tags(rapidjson::kObjectType);
    //            invite_tags.AddMember("invite_surface", "unknown", allocator);
    //            entry.AddMember("inviteTags", invite_tags, allocator);
    //            
    //            entry.AddMember("userType", "NUCLEUS_USER", allocator);
    //            entry.AddMember("displayName", rapidjson::Value(display_name.c_str(), allocator), allocator);
    //            entry.AddMember("personaId", std::stoll(friend_id), allocator);
    //            entry.AddMember("nickName", rapidjson::Value(nickname.c_str(), allocator), allocator);
    //            
    //            entries.PushBack(entry, allocator);
    //        }
    //        doc.AddMember("entries", entries, allocator);

    //        rapidjson::Value paging(rapidjson::kObjectType);
    //        paging.AddMember("size", entries.Size(), allocator);
    //        paging.AddMember("offset", 0, allocator);
    //        paging.AddMember("totalSize", entries.Size(), allocator);
    //        doc.AddMember("pagingInfo", paging, allocator);

    //        rapidjson::StringBuffer buffer;
    //        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
    //        doc.Accept(writer);

    //        ctx->set_response_http_code(200);
    //        cb(buffer.GetString());

    //    } catch (const std::exception& e) {
    //        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FRIENDS,
    //            "[FRIENDS] Error handling outbound invites: %s", e.what());
    //        ctx->set_response_http_code(500);
    //        cb("{\"message\":\"Internal server error\"}");
    //    }
    //}

    //void Friends::handle_accept_friend_request(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
    //    const evpp::http::HTTPSendResponseCallback& cb) {
    //    try {
    //        logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_FRIENDS,
    //            "[FRIENDS] Accept friend request from %s: %s", ctx->remote_ip().data(), ctx->uri().data());

    //        std::string uri = ctx->uri();
    //        std::string user_id;
    //        std::string friend_id;

    //        // Extract user ID and friend ID from URL: /friends/2/users/{userId}/invitations/inbound/{friendId}
    //        std::regex pattern(R"(/friends/2/users/(\d+)/invitations/inbound/(\d+))");
    //        std::smatch matches;
    //        if (std::regex_search(uri, matches, pattern) && matches.size() > 2) {
    //            user_id = matches[1].str();
    //            friend_id = matches[2].str();
    //        }

    //        if (user_id.empty() || friend_id.empty()) {
    //            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FRIENDS,
    //                "[FRIENDS] Invalid user ID or friend ID in URL: %s", uri.c_str());
    //            ctx->set_response_http_code(400);
    //            cb("{\"message\":\"Invalid user ID or friend ID\"}");
    //            return;
    //        }

    //        // Validate auth token
    //        std::string access_token = helpers::extract_access_token(ctx, logger::LOG_LABEL_FRIENDS, "[FRIENDS]");
    //        if (access_token.empty()) {
    //            ctx->set_response_http_code(401);
    //            cb("{\"message\":\"Missing or invalid access token\"}");
    //            return;
    //        }

    //        auto& db = tsto::database::Database::get_instance();
    //        std::string email;
    //        std::string stored_user_id;

    //        if (!db.get_email_by_token(access_token, email) || !db.get_user_id(email, stored_user_id)) {
    //            ctx->set_response_http_code(401);
    //            cb("{\"message\":\"Invalid access token\"}");
    //            return;
    //        }

    //        // Accept the friend request
    //        if (!db.accept_friend_request(user_id, friend_id)) {
    //            ctx->set_response_http_code(500);
    //            cb("{\"message\":\"Failed to accept friend request\"}");
    //            return;
    //        }

    //        // Return 204 No Content on success
    //        ctx->set_response_http_code(204);
    //        cb("");

    //    } catch (const std::exception& e) {
    //        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FRIENDS,
    //            "[FRIENDS] Error accepting friend request: %s", e.what());
    //        ctx->set_response_http_code(500);
    //        cb("{\"message\":\"Internal server error\"}");
    //    }
    //}

} 