#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto/includes/session.hpp"

namespace tsto::auth {
    class Auth {
    public:
     
        static void handle_check_token(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_connect_auth(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_connect_token(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_connect_tokeninfo(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);



    };
}