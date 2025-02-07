#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto_server.hpp"


namespace tsto::device{
    class Device {
    public:
        static void handle_device_registration(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_get_anon_uid(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_get_device_id(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_validate_device_id(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);

    };
}