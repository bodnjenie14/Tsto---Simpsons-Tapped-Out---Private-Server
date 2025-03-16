#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include "debugging/serverlog.hpp"
#include "utilities/cryptography.hpp"

namespace tsto::helpers {


    inline void log_request(const evpp::http::ContextPtr& ctx) {
        std::string body_str(ctx->body().data(), ctx->body().size());

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_SERVER_HTTP,
            "Request %s - Body: %s",
            ctx->uri().c_str(),
            body_str.c_str());
    }

    inline std::string generate_random_id() {
        return utils::cryptography::random::get_challenge();
    }

    inline std::string generate_session_key() {
        return utils::cryptography::random::get_challenge();
    }


}