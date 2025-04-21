#pragma once

#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/event_loop.h>

namespace tsto::dashboard {

    class UserSearch {
    public:
        static void handle_search_users(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_get_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_update_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_delete_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_get_all_users(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);
    };

}   
