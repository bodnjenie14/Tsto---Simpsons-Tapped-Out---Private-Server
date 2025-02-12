#include <std_include.hpp>

#include "response_headers.hpp"

namespace tsto {

    namespace headers {

        void set_json_response(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Content-Type", "application/json");
            // ctx->AddResponseHeader("Server", "TSTO-Server/1.0");
        }

        void set_protobuf_response(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Content-Type", "application/x-protobuf");
            // ctx->AddResponseHeader("Server", "TSTO-Server/1.0");
        }

        void set_xml_response(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Content-Type", "application/xml");
            // ctx->AddResponseHeader("Server", "TSTO-Server/1.0");
        }

        void set_html_response(const evpp::http::ContextPtr& ctx) {
            ctx->AddResponseHeader("Content-Type", "application/html");
            // ctx->AddResponseHeader("Server", "TSTO-Server/1.0");
        }

    } // namespace headers

} // namespace tsto