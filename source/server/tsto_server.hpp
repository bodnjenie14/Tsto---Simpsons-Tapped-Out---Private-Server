#pragma once
#include <std_include.hpp>
#include <memory>
#include <evpp/http/http_server.h>
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>

// Protobuf includes
#include "build/AuthData.pb.h"
#include "build/ClientConfigData.pb.h"
#include "build/ClientLog.pb.h"
#include "build/ClientMetrics.pb.h"
#include "build/ClientTelemetry.pb.h"
#include "build/Common.pb.h"
#include "build/CustomerServiceData.pb.h"
#include "build/Error.pb.h"
#include "build/GambleData.pb.h"
#include "build/GameplayConfigData.pb.h"
#include "build/GetFriendData.pb.h"
#include "build/LandData.pb.h"
#include "build/MatchmakingData.pb.h"
#include "build/OffersData.pb.h"
#include "build/PurchaseData.pb.h"
#include "build/WholeLandTokenData.pb.h"

// Utility includes
#include "utilities/cryptography.hpp"
#include "utilities/http.hpp"
#include "utilities/string.hpp"
#include "utilities/serialization.hpp"
#include "utilities/configuration.hpp"

// Debugging includes
#include "debugging/serverlog.hpp"

// Headers includes
#include "headers/response_headers.hpp"  

#include "tsto/includes/session.hpp"
#include "tsto/includes/helpers.hpp"


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

        // Docker configuration
        public:
        bool docker_enabled_;    // Whether Docker mode is enabled
        uint16_t docker_port_;   // Docker port mapping (default 8080)

        bool reverse_proxy_enabled_;
        bool reverse_proxy_trust_headers_;
        std::string reverse_proxy_force_host_;
        int reverse_proxy_force_port_;
        std::string reverse_proxy_force_protocol_;

        std::string get_server_address() const {
            // Use the server_ip_ directly - this is set during initialization
            // from the configuration in server_startup.cpp
            return server_ip_;
        }

        std::string get_protocol(const evpp::http::ContextPtr& ctx) const {
            // If reverse proxy is enabled and we have a forced protocol, use that
            if (reverse_proxy_enabled_ && !reverse_proxy_force_protocol_.empty()) {
                return reverse_proxy_force_protocol_;
            }
            
            // Check for X-Forwarded-Proto header if we trust headers
            if (reverse_proxy_enabled_ && reverse_proxy_trust_headers_) {
                const char* forwarded_proto = ctx->FindRequestHeader("X-Forwarded-Proto");
                if (forwarded_proto && *forwarded_proto) {
                    return forwarded_proto;
                }
            }
            
            // Default to http
            return "http";
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
        void handle_progreg_code(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        void handle_proto_currency(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        void handle_plugin_event_protoland(evpp::EventLoop*, const evpp::http::ContextPtr&, const evpp::http::HTTPSendResponseCallback&);
      
        std::string generate_random_id();
        std::string generate_session_key() {
            return utils::cryptography::random::get_challenge();
        }
    };

}