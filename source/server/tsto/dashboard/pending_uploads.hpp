#pragma once
#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/event_loop.h>

namespace tsto::dashboard {

    void handle_public_town_uploader(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_submit_town_upload(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_get_pending_towns(evpp::EventLoop* loop, const std::shared_ptr<evpp::http::Context>& ctx, const std::function<void(const std::string&)>& cb);
    void handle_approve_pending_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_reject_pending_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
    void handle_get_user_pending_towns(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);

} 
