#pragma once
#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <string>
#include <chrono>

using evpp::http::Context;
using ContextPtr = std::shared_ptr<Context>;
using HTTPSendResponseCallback = std::function<void(const std::string& response)>;

namespace tsto {
namespace dashboard {

class Dashboard {
public:
    static void set_server_info(const std::string& ip, uint16_t port);
    static void handle_dashboard(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_server_restart(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_server_stop(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_update_initial_donuts(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_set_event(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_force_save_protoland(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_update_dlc_directory(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_update_server_ip(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_update_server_port(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_browse_directory(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_list_users(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_edit_user_currency(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_get_user_save(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_save_user_save(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_upload_town_file(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_server_status(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_current_event(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_events_list(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void handle_dashboard_data(evpp::EventLoop* loop, const ContextPtr& ctx, const HTTPSendResponseCallback& cb);
    static void restart_server();

private:
    static std::string server_ip_;
    static uint16_t server_port_;
    static std::chrono::system_clock::time_point start_time_;
    static void restart_server_logic();
};

} // namespace dashboard
} // namespace tsto