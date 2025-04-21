#pragma once

#include <evpp/http/context.h>
#include <evpp/event_loop.h>
#include <string>
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto/includes/session.hpp"

namespace tsto::auth {
    class Auth {
    public:
        static void handle_check_token(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_connect_auth(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_connect_token(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_connect_tokeninfo(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        
    private:
        static std::string extract_token_from_headers(const evpp::http::ContextPtr& ctx);
        
        static bool extract_user_info(const evpp::http::ContextPtr& ctx, 
                                     std::string& email, 
                                     std::string& user_id, 
                                     std::string& mayhem_id, 
                                     std::string& access_token);
                                     
        static bool get_land_token_for_user(const std::string& email, std::string& land_token);
    };
}
