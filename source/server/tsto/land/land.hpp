#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto_server.hpp"
#include "tsto/includes/session.hpp"
#include "LandData.pb.h"
#include <evpp/http/context.h>

namespace tsto::land {
    class Land {
    public:
        static void handle_proto_whole_land_token(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        static void handle_protoland(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_tutorial_land(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_extraland_update(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb, const std::string& land_id);
        static void handle_delete_token(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_town_operations(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static bool save_town();
        static bool static_load_town();
        static bool load_town_by_email(const std::string& email);
        static bool save_town_as(const std::string& email);
        static bool import_town_file(const std::string& source_path, const std::string& email);
        static bool copy_town(const std::string& source_email, const std::string& target_email);
        static void create_default_currency_file(const std::string& email);

        Land() = default;
        ~Land() = default;
        
        void set_email(const std::string& email) { email_ = email; }
        std::string get_filename() const { return email_ + ".pb"; }
        bool instance_load_town();
        bool instance_save_town();

    private:
        std::string email_;
        static void create_blank_town();
        static bool validate_land_data(const Data::LandMessage& land_data);
        static void handle_get_request(const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&, const std::string&);
        static void handle_put_request(const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_post_request(const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
    };
}