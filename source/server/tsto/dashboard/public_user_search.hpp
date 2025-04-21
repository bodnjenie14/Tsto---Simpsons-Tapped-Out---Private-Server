#pragma once

#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/event_loop.h>

namespace tsto::dashboard {

    class PublicUserSearch {
    public:
        static void handle_search_public_users(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_get_public_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_update_public_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_delete_public_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        static void handle_get_all_public_users(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);
    };

}   
