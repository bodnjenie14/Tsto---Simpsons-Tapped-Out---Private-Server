#include <std_include.hpp>
#include "tracking.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "ClientTelemetry.pb.h"

namespace tsto::tracking {

    void Tracking::handle_tracking_log(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_TRACKING,
                "[TRACKING LOG] Request received from %s",
                ctx->remote_ip().data());

            com::ea::simpsons::client::log::ClientLogMessage client_log;
            if (!client_log.ParseFromString(std::string(ctx->body().data(), ctx->body().size()))) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_TRACKING,
                    "[TRACKING LOG] Failed to parse protobuf message");
                ctx->set_response_http_code(400);
                cb("");
                return;
            }

            std::ofstream log_file("tracking_logs.txt", std::ios::app);
            if (log_file.is_open()) {
                time_t now = time(0);
                log_file << std::ctime(&now);
                log_file << "Client Log Details:" << std::endl;
                log_file << "\tTimestamp: " << client_log.time_stamp() << std::endl;
                log_file << "\tSeverity: " << client_log.severity() << std::endl;
                log_file << "\tSource: " << client_log.source() << std::endl;
                log_file << "\tText: " << client_log.text() << std::endl;

                if (client_log.has_userid()) {
                    log_file << "\tUserId: " << client_log.userid() << std::endl;
                }
                if (client_log.has_connectiontype()) {
                    log_file << "\tConnection Type: " << client_log.connectiontype() << std::endl;
                }
                if (client_log.has_serverlogfields()) {
                    log_file << "\tServer Log Fields present" << std::endl;
                }

                log_file << "----------------------------------------" << std::endl;
                log_file.close();
            }
#ifdef DEBUG

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                "[TRACKING LOG] Parsed Message:\n"
                "\tTimestamp: %lld\n"
                "\tSeverity: %d\n"
                "\tSource: %s\n"
                "\tText: %s",
                client_log.time_stamp(),
                client_log.severity(),
                client_log.source().c_str(),
                client_log.text().c_str());

            if (client_log.has_userid()) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                    "\tUserId: %s", client_log.userid().c_str());
            }
            if (client_log.has_connectiontype()) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                    "\tConnection Type: %d", client_log.connectiontype());
            }

            if (client_log.has_serverlogfields()) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                    "\tServer Log Fields: present");
            }

#endif 


            const char* response = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><Resources><URI>OK</URI></Resources>";
            headers::set_xml_response(ctx);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_TRACKING,
                "[TRACKING LOG] Response sent to %s",
                ctx->remote_ip().data());

            cb(response);
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_TRACKING,
                "[TRACKING LOG] Error: %s", e.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Tracking::handle_tracking_metrics(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {


            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_TRACKING, "[TRACKING METRICS] Request received from %s", ctx->remote_ip().data());

            //crashing logger
            //logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_TRACKING, "[TRACKING METRICS] Received - Type: %d, Name: %s, Value: %s",
            //    metrics.type(), metrics.name().c_str(), metrics.value());

            com::ea::simpsons::client::metrics::ClientMetricsMessage metrics;
            if (!metrics.ParseFromString(std::string(ctx->body().data(), ctx->body().size()))) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_TRACKING,
                    "[TRACKING METRICS] Failed to parse protobuf message");
                ctx->set_response_http_code(400);
                cb("");
                return;
            }

            std::ofstream log_file("tracking_logs.txt", std::ios::app);
            if (log_file.is_open()) {
                time_t now = time(0);
                log_file << std::ctime(&now);
                log_file << "Metrics Details:" << std::endl;
                log_file << "\tType: " << metrics.type() << std::endl;
                log_file << "\tName: " << metrics.name() << std::endl;
                log_file << "\tValue: " << metrics.value() << std::endl;
                log_file << "----------------------------------------" << std::endl;
                log_file.close();
            }

#ifdef DEBUG

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                "[TRACKING METRICS] Parsed Message:\n"
                "\tType: %d\n"
                "\tName: %s\n"
                "\tValue: %f",
                metrics.type(),
                metrics.name().c_str(),
                metrics.value());

#endif
            Data::ExtraLandResponse response;
            headers::set_protobuf_response(ctx);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_TRACKING,
                "[TRACKING METRICS] Response sent to %s",
                ctx->remote_ip().data());

            cb(response.SerializeAsString());
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_TRACKING,
                "[TRACKING METRICS] Error: %s", e.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Tracking::handle_core_log_event(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_TRACKING,
                "[CORE LOG] Request received from %s",
                ctx->remote_ip().data());

            std::ofstream log_file("tracking_logs.txt", std::ios::app);
            if (log_file.is_open()) {
                time_t now = time(0);
                log_file << std::ctime(&now);
                log_file << "URI: " << ctx->uri() << std::endl;
                log_file << "IP: " << ctx->remote_ip() << std::endl;
                log_file << "Body size: " << ctx->body().size() << " bytes" << std::endl;
                log_file << "Payload: " << std::string(ctx->body().data(), std::min(size_t(1000), ctx->body().size())) << std::endl;
                log_file << "----------------------------------------" << std::endl;
                log_file.close();
            }

#ifdef DEBUG

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                "[CORE LOG] Query Parameters:\n"
                "localization: %s\n"
                "appLang: %s\n"
                "deviceLocale: %s\n"
                "deviceLanguage: %s\n"
                "appVer: %s\n"
                "hwId: %s",
                ctx->GetQuery("localization").empty() ? "(missing)" : ctx->GetQuery("localization").c_str(),
                ctx->GetQuery("appLang").empty() ? "(missing)" : ctx->GetQuery("appLang").c_str(),
                ctx->GetQuery("deviceLocale").empty() ? "(missing)" : ctx->GetQuery("deviceLocale").c_str(),
                ctx->GetQuery("deviceLanguage").empty() ? "(missing)" : ctx->GetQuery("deviceLanguage").c_str(),
                ctx->GetQuery("appVer").empty() ? "(missing)" : ctx->GetQuery("appVer").c_str(),
                ctx->GetQuery("hwId").empty() ? "(missing)" : ctx->GetQuery("hwId").c_str());

            // body payload
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_TRACKING,
                "[CORE LOG] Body (json):\n%s",
                ctx->body().empty() ? "(empty)" : std::string(ctx->body().data(), ctx->body().size()).c_str());

#endif 

            rapidjson::Document doc;
            doc.SetObject();
            doc.AddMember("status", "ok", doc.GetAllocator());

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            doc.Accept(writer);

            headers::set_json_response(ctx);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_TRACKING,
                "[CORE LOG] Response sent to %s",
                ctx->remote_ip().data());

            cb(buffer.GetString());
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_TRACKING,
                "[CORE LOG] Error: %s", e.what());
            ctx->set_response_http_code(500);
            cb(R"({"status": "error", "message": "Internal Server Error"})");
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