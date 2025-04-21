#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

// Protobuf includes
#include "AuthData.pb.h"
#include "ClientConfigData.pb.h"
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "ClientTelemetry.pb.h"
#include "Common.pb.h"
#include "CustomerServiceData.pb.h"
#include "Error.pb.h"
#include "GambleData.pb.h"
#include "GameplayConfigData.pb.h"
#include "GetFriendData.pb.h"
#include "LandData.pb.h"
#include "MatchmakingData.pb.h"
#include "OffersData.pb.h"
#include "PurchaseData.pb.h"
#include "WholeLandTokenData.pb.h"

// Utility includes
#include "cryptography.hpp"
#include "http.hpp"
#include "string.hpp"
#include <serialization.hpp>

// Debugging includes
#include "debugging/serverlog.hpp"

// Headers includes
#include "headers/response_headers.hpp"  

#include "tsto/includes/session.hpp"
#include "tsto/includes/helpers.hpp"
#include "tsto/land/backup.hpp"

namespace server::dispatcher::http {
    class Dispatcher;
}

namespace tsto {

    class TSTOServer {
        friend class server::dispatcher::http::Dispatcher;

    public:
        ~TSTOServer() = default;

        bool initialize(uint16_t port);
        void start();
        void stop();

        std::string server_ip_;  // Configured through server-config.json
        uint16_t server_port_;   // Configured through server-config.json


        std::string get_server_address() const {
            if (server_port_ == 80) {
                return server_ip_;
            }
            return server_ip_ + ":" + std::to_string(server_port_);
        }


    private:
        // Server config
        bool debug_;
        evpp::http::Server server_;

        //server ip stuff


        //shite
        void set_server_ip(const std::string& ip) { server_ip_ = ip; }
        void set_server_port(uint16_t port) { server_port_ = port; }

        //void handle_dashboard(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        //void handle_server_restart(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        //void handle_server_stop(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        //void handle_update_initial_donuts(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        //void handle_update_current_donuts(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        //void handle_set_event(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        //void handle_force_save_protoland(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        //void handle_update_dlc_directory(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);




        void handle_root(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        void handle_get_direction(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb, const std::string& platform);
        void handle_lobby_time(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        void handle_geoage_requirements(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        void handle_friend_data(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        void handle_pin_events(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        void handle_plugin_event(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
        void handle_friend_data_origin(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        void handle_proto_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        void handle_plugin_event_protoland(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        void handle_links(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

        // Initialize the town backup system

    private:
        std::string generate_random_id();
        std::string generate_session_key() {
            return utils::cryptography::random::get_challenge();
        }
    };

}