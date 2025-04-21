#pragma once
#include <map>
#include <string>
#include <chrono>
#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/event_loop.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "../auth/user_roles.hpp"
#include "../../security/security.hpp"

namespace tsto::dashboard {

    class DashboardConfig {
    public:
        static void handle_update_backup_settings(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_update_api_settings(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_update_land_settings(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_update_security_settings(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_update_initial_donuts(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_get_dashboard_config(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);

    private:
 
    };

}