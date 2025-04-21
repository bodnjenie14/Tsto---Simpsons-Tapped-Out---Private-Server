#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto_server.hpp"
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
        static void create_blank_town();
        static void initialize_config();

        Land() = default;
        ~Land() = default;

        void set_email(const std::string& email) { email_ = email; }
        std::string get_email() const { return email_; }

        void set_access_token(const std::string& token) { access_token_ = token; }
        std::string get_access_token() const { return access_token_; }

        void set_user_id(const std::string& id) { user_id_ = id; }
        std::string get_user_id() const { return user_id_; }

        void set_mayhem_id(const std::string& id) { mayhem_id_ = id; }
        std::string get_mayhem_id() const { return mayhem_id_; }

        void set_town_path(const std::string& path) { town_path_ = path; }
        std::string get_town_path() const { return town_path_; }
        std::string get_town_filename() const {
            if (!town_path_.empty()) {
                return town_path_.substr(town_path_.find_last_of("/\\") + 1);
            }
            return email_ + ".pb";
        }

        Data::LandMessage& get_land_proto() { return land_proto_; }
        const Data::LandMessage& get_land_proto() const { return land_proto_; }

        bool instance_load_town();
        bool instance_save_town();

    private:
        std::string email_;
        std::string access_token_;
        std::string user_id_;
        std::string mayhem_id_;
        std::string town_path_;
        Data::LandMessage land_proto_; // Store the land proto directly in the instance
        static bool validate_land_data(const Data::LandMessage& land_data);
        static void handle_get_request(const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&, const std::string&);
        static void handle_put_request(const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        static void handle_post_request(const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
    };
}