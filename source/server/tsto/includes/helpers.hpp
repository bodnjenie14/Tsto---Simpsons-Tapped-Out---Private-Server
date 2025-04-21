#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "debugging/serverlog.hpp"
#include "cryptography.hpp"
#include "tsto/database/database.hpp"
#include <iomanip>
#include <sstream>
#include <chrono>

namespace tsto::helpers {

    inline void log_request(const evpp::http::ContextPtr& ctx) {
        std::string body_str(ctx->body().data(), ctx->body().size());

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
            "Request %s - Body: %s",
            ctx->uri().c_str(),
            body_str.c_str());
    }

    inline std::string generate_random_id() {
        return utils::cryptography::random::get_challenge();
    }

    inline std::string generate_session_key() {
        return utils::cryptography::random::get_challenge();
    }


    // non used func hahaha
    inline std::string extract_access_token(const evpp::http::ContextPtr& ctx, 
                                           logger::LogLabel log_label = logger::LOG_LABEL_AUTH,
                                           const char* log_prefix = "[TOKEN]") {
        std::string access_token;
        
        const char* token_header = ctx->FindRequestHeader("access_token");
        if (!token_header) {
            token_header = ctx->FindRequestHeader("Access-Token");
        }
        if (!token_header) {
            token_header = ctx->FindRequestHeader("Access_Token");
        }
        
        if (token_header) {
            access_token = token_header;
            logger::write(logger::LOG_LEVEL_DEBUG, log_label,
                "%s Found access token in header: %s",
                log_prefix, access_token.substr(0, 10).c_str());
            return access_token;
        }
        
        token_header = ctx->FindRequestHeader("mh_auth_params");
        if (!token_header) {
            token_header = ctx->FindRequestHeader("fallback_auth_params");
        }
        if (!token_header) {
            token_header = ctx->FindRequestHeader("nucleus_token");
        }
        if (!token_header) {
            token_header = ctx->FindRequestHeader("ingly_token");
        }
        
        if (token_header) {
            std::string auth_str(token_header);
            if (auth_str.find("Bearer ") == 0) {
                access_token = auth_str.substr(7);
                access_token.erase(0, access_token.find_first_not_of(" \t\r\n"));
            } else {
                access_token = auth_str;
            }
            
            logger::write(logger::LOG_LEVEL_DEBUG, log_label,
                "%s Found access token in game-specific header: %s",
                log_prefix, access_token.substr(0, 10).c_str());
            return access_token;
        }
        
        const char* auth_header = ctx->FindRequestHeader("Authorization");
        if (auth_header) {
            std::string auth_str(auth_header);
            auth_str.erase(0, auth_str.find_first_not_of(" \t\r\n"));
            auth_str.erase(auth_str.find_last_not_of(" \t\r\n") + 1);
            
            if (auth_str.find("Bearer ") == 0) {
                access_token = auth_str.substr(7);
                access_token.erase(0, access_token.find_first_not_of(" \t\r\n"));
                
                if (access_token.empty()) {
                    logger::write(logger::LOG_LEVEL_ERROR, log_label,
                        "%s Invalid Authorization header format: Bearer token is empty",
                        log_prefix);
                    return "";
                }
                
                logger::write(logger::LOG_LEVEL_DEBUG, log_label,
                    "%s Found access token in Authorization header: %s",
                    log_prefix, access_token.substr(0, 10).c_str());
                return access_token;
            } else {
                access_token = auth_str;
                logger::write(logger::LOG_LEVEL_DEBUG, log_label,
                    "%s Found non-Bearer access token in Authorization header: %s",
                    log_prefix, access_token.substr(0, 10).c_str());
                return access_token;
            }
        }
        
        auto query_token = ctx->GetQuery("access_token");
        if (!query_token.empty()) {
            access_token = query_token;
            logger::write(logger::LOG_LEVEL_DEBUG, log_label,
                "%s Found access token in URL query: %s",
                log_prefix, access_token.substr(0, 10).c_str());
            return access_token;
        }
        
        query_token = ctx->GetQuery("token");
        if (!query_token.empty()) {
            access_token = query_token;
            logger::write(logger::LOG_LEVEL_DEBUG, log_label,
                "%s Found access token in URL query (token parameter): %s",
                log_prefix, access_token.substr(0, 10).c_str());
            return access_token;
        }
        
        logger::write(logger::LOG_LEVEL_DEBUG, log_label,
            "%s No access token found in request", log_prefix);
        return "";
    }

    inline std::string format_iso8601(int64_t timestamp_ms) {
        using namespace std::chrono;
        
        auto timepoint = system_clock::time_point(milliseconds(timestamp_ms));
        auto time_t = system_clock::to_time_t(timepoint);
        auto ms = duration_cast<milliseconds>(timepoint.time_since_epoch()) % 1000;

        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%FT%T") 
           << '.' << std::setfill('0') << std::setw(3) << ms.count() 
           << 'Z';

        return ss.str();
    }

    inline bool validate_token_and_get_user(const std::string& token, 
                                           std::string& email, 
                                           std::string& user_id,
                                           logger::LogLabel log_label = logger::LOG_LABEL_AUTH,
                                           const char* log_prefix = "[TOKEN]") {
        if (token.empty()) {
            logger::write(logger::LOG_LEVEL_DEBUG, log_label,
                "%s Empty token provided", log_prefix);
            return false;
        }
        
        auto& db = tsto::database::Database::get_instance();
        
        bool valid = db.validate_access_token(token, email);
        if (!valid || email.empty()) {
            logger::write(logger::LOG_LEVEL_DEBUG, log_label,
                "%s Invalid token or no email found: %s", 
                log_prefix, token.substr(0, 10).c_str());
            return false;
        }
        
        if (!db.get_user_id(email, user_id) || user_id.empty()) {
            logger::write(logger::LOG_LEVEL_DEBUG, log_label,
                "%s No user ID found for email: %s", 
                log_prefix, email.c_str());
            return false;
        }
        
        logger::write(logger::LOG_LEVEL_DEBUG, log_label,
            "%s Valid token for user: %s (ID: %s)", 
            log_prefix, email.c_str(), user_id.c_str());
        return true;
    }

}