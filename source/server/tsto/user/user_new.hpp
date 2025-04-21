#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto_server.hpp"

#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/http/service.h>

namespace tsto::user {

std::string extract_access_token(const evpp::http::ContextPtr& ctx, const std::string& log_prefix);

std::string extract_user_id_from_url(const evpp::http::ContextPtr& ctx, const std::string& log_prefix);

bool get_user_info_from_token(const std::string& access_token, 
                             std::string& email, 
                             std::string& user_id,
                             std::string& display_name,
                             const std::string& log_prefix);

class User {
public:
    static void handle_me_personas(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
    
    static void handle_mh_users(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
    
    static void handle_mh_userstats(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
};

}   
