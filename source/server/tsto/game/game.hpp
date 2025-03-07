#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include <rapidjson/document.h>
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto_server.hpp"

namespace tsto::game {

    class Game {
    public:
        static void handle_gameplay_config(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_proto_client_config(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);

        
    private:
    };
}