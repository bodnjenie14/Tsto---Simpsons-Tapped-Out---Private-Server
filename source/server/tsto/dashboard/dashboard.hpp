#pragma once
#include <map>
#include <string>
#include <chrono>
#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/event_loop.h>

namespace tsto::dashboard {

    class Dashboard {
    public:
        static void set_server_info(const std::string& ip, uint16_t port) {
            server_ip_ = ip;
            server_port_ = port;
        }

        static void handle_dashboard(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_server_restart(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_server_stop(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_update_initial_donuts(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_update_current_donuts(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_set_event(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_force_save_protoland(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_update_dlc_directory(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_update_server_ip(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_update_server_port(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_browse_directory(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);

    private:
        static std::string server_ip_;
        static uint16_t server_port_;
    };

}