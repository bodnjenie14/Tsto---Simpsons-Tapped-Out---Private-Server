#include <std_include.hpp>

#include "response_headers.hpp"

namespace tsto {

    namespace headers {

        //CORS headers to any response
        void add_cors_headers(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Access-Control-Allow-Origin", "*");
            ctx->AddResponseHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            ctx->AddResponseHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, nucleus_token, mh_auth_params, X-Requested-With");
            ctx->AddResponseHeader("Access-Control-Allow-Credentials", "true");
            ctx->AddResponseHeader("Access-Control-Max-Age", "3600");
        }

        void set_json_response(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Content-Type", "application/json");
            add_cors_headers(ctx);
        }

        void set_protobuf_response(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Content-Type", "application/x-protobuf");
            add_cors_headers(ctx);
        }

        void set_xml_response(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Content-Type", "application/xml");
            add_cors_headers(ctx);
        }

    } 

} 