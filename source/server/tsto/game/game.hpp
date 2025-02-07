#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto_server.hpp"


namespace tsto::game {
    class Game {
    public:

        static void handle_get_direction(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb, const std::string& platform);
        static void handle_proto_client_config(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_gameplay_config(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);

    };
}