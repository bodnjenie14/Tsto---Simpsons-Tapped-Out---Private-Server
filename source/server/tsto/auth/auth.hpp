#pragma once

#include <evpp/http/context.h>
#include <evpp/event_loop.h>
#include <string>
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto/includes/session.hpp"

namespace tsto::auth {
    std::string generate_access_token(const std::string& type, const std::string& user_id);
    std::string generate_access_code(const std::string& user_id);

    class Auth {
    public:
        static void handle_check_token(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_connect_auth(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_connect_token(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_connect_tokeninfo(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static std::string generate_random_code();

        static std::string generate_random_token(const std::string& user_id);
        
        static std::string generate_access_token(const std::string& user_id);
        
        static std::string generate_typed_access_token(const std::string& type, const std::string& user_id);
        
        static std::string generate_access_code(const std::string& user_id);
    };
}