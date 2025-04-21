#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include <rapidjson/document.h>
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto_server.hpp"

namespace tsto::friends {

    class Friends {
    public:
        //static void handle_get_inbound_invites(evpp::EventLoop* loop,
        //    const evpp::http::ContextPtr& ctx,
        //    const evpp::http::HTTPSendResponseCallback& cb);

        //static void handle_get_outbound_invites(evpp::EventLoop* loop,
        //    const evpp::http::ContextPtr& ctx,
        //    const evpp::http::HTTPSendResponseCallback& cb);

        //static void handle_accept_friend_request(evpp::EventLoop* loop,
        //    const evpp::http::ContextPtr& ctx,
        //    const evpp::http::HTTPSendResponseCallback& cb);

    private:
        static std::string extract_user_id_from_uri(const std::string& uri);
        static std::string extract_friend_id_from_uri(const std::string& uri);
        static void send_error_response(const evpp::http::HTTPSendResponseCallback& cb,
            int status_code,
            const std::string& error_message);
    };
}