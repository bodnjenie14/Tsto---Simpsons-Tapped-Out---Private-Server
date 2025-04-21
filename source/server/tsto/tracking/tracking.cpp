#include <std_include.hpp>
#include "tracking.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "ClientTelemetry.pb.h"
#include "ClientLog.pb.h"
#include "ClientMetrics.pb.h"
#include "AuthData.pb.h"

namespace tsto::tracking {

    void Tracking::handle_tracking_log(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            // Parse protobuf message
            com::ea::simpsons::client::log::ClientLogMessage req;
            if (!req.ParseFromString(std::string(ctx->body().data(), ctx->body().size()))) {
                ctx->set_response_http_code(400);
                cb("");
                return;
            }

            bool verbose_logging = utils::configuration::ReadBoolean("Tracking", "VerboseLogging", false);
            if (verbose_logging) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                    "[TRACKING LOG] Message: %s", req.DebugString().c_str());
            }

            // Return XML response
            const char* xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Resources><URI>OK</URI></Resources>";
            headers::set_xml_response(ctx);
            cb(xml);
        }
        catch (const std::exception& e) {
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Tracking::handle_core_log_event(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            bool verbose_logging = utils::configuration::ReadBoolean("Tracking", "VerboseLogging", false);
            if (verbose_logging) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                    "[CORE LOG] Body: %s",
                    ctx->body().empty() ? "(empty)" : std::string(ctx->body().data(), ctx->body().size()).c_str());
            }

            // Return JSON response
            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("status", "ok", doc.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            headers::set_json_response(ctx);
            cb(buffer.GetString());
        }
        catch (const std::exception& e) {
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Tracking::handle_pin_events(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            bool verbose_logging = utils::configuration::ReadBoolean("Tracking", "VerboseLogging", false);
            if (verbose_logging) {
                // Get JSON body and log it
                rapidjson::Document doc;
                if (!ctx->body().empty() && !doc.Parse(ctx->body().data()).HasParseError()) {
                    rapidjson::StringBuffer buffer;
                    rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                    doc.Accept(writer);
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                        "[PIN EVENTS] JSON: %s", buffer.GetString());
                }
            }

            // Return JSON response
            headers::set_json_response(ctx);
            cb(R"({"status": "ok"})");
        }
        catch (const std::exception& e) {
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Tracking::handle_tracking_metrics(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            // Parse protobuf message
            com::ea::simpsons::client::metrics::ClientMetricsMessage req;
            if (!req.ParseFromString(std::string(ctx->body().data(), ctx->body().size()))) {
                ctx->set_response_http_code(400);
                cb("");
                return;
            }

            bool verbose_logging = utils::configuration::ReadBoolean("Tracking", "VerboseLogging", false);
            if (verbose_logging) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                    "[TRACKING METRICS] Message: %s", req.DebugString().c_str());
            }

            // Create token data
            auto& session = tsto::Session::get();
            Data::TokenData data;
            data.set_sessionkey(session.token_session_key);
            data.set_expirationdate(0);

            // Return XML response
            const char* xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Resources><URI>OK</URI></Resources>";
            headers::set_xml_response(ctx);
            cb(xml);
        }
        catch (const std::exception& e) {
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Tracking::handle_client_telemetry(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_TRACKING,
                "[CLIENT TELEMETRY] Request received from %s",
                ctx->remote_ip().data());

            com::ea::simpsons::client::telemetry::ClientTelemetryMessage telemetry;
            if (!telemetry.ParseFromString(std::string(ctx->body().data(), ctx->body().size()))) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_TRACKING,
                    "[CLIENT TELEMETRY] Failed to parse protobuf message");
                ctx->set_response_http_code(400);
                cb("");
                return;
            }

            const char* response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Resources><URI>OK</URI></Resources>";
            headers::set_xml_response(ctx);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_TRACKING,
                "[CLIENT TELEMETRY] Response sent to %s",
                ctx->remote_ip().data());

            cb(response);
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_TRACKING,
                "[CLIENT TELEMETRY] Error: %s", e.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }
}