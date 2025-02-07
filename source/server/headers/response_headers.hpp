#pragma once
#include <evpp/http/http_server.h>

namespace tsto {

    namespace headers {

        void set_json_response(const evpp::http::ContextPtr& ctx);
        void set_protobuf_response(const evpp::http::ContextPtr& ctx);
        void set_xml_response(const evpp::http::ContextPtr& ctx);

    } 

} 