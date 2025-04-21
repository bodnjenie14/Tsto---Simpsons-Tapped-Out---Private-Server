#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto_server.hpp"

#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/http/service.h>


//static

namespace tsto::user {
    class User {
    public:

        static void handle_me_personas(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_mh_users(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_mh_userstats(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);

    };
}