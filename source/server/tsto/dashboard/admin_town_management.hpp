#pragma once

#include <evpp/http/context.h>
#include <evpp/http/http_server.h>

namespace tsto::dashboard {

    void handle_admin_import_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb);
        
    void handle_admin_export_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb);
        
    void handle_admin_delete_town(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb);
        
    void handle_admin_save_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb);
        
    void handle_admin_get_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb);
        
    void handle_admin_view_user(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb);

}   
