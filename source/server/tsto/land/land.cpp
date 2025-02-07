#include <std_include.hpp>
#include "land.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/http/service.h>
#include <3rdparty/libevent/include/event2/http.h>
#include <compression.hpp>

namespace tsto::land {

    void Land::handle_proto_whole_land_token(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();
            std::string token;


            size_t token_start = uri.find("/checkToken/") + 11;  // Length of "/checkToken/"
            size_t token_end = uri.find("/protoWholeLandToken/");
            if (token_start != std::string::npos && token_end != std::string::npos) {
                token = uri.substr(token_start, token_end - token_start);
            }

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_LAND, "[CHECK TOKEN] Request from %s: %s", ctx->remote_ip().data(), ctx->uri().data());

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[CHECK TOKEN] Request params:\n"
                "URI: %s\n"
                "Token: %s",
                ctx->uri().c_str(),
                token.c_str()
            );

            auto& session = tsto::Session::get();

            Data::TokenData response;
            response.set_sessionkey(session.session_key);
            response.set_expirationdate(0);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_LAND, "[CHECK TOKEN] Sending response for token: %s", token.c_str());

            headers::set_protobuf_response(ctx);
            cb(utils::serialization::serialize_protobuf(response));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND, "[CHECK TOKEN] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }


    bool Land::load_town() {
        auto& session = tsto::Session::get();
        std::filesystem::path town_file_path = "towns/mytown.pb";

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME, "[LAND] Attempting to load %s", town_file_path.string().c_str());

        if (!std::filesystem::exists(town_file_path)) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME, "[LAND] No existing town found at %s, creating new town", town_file_path.string().c_str());
            create_blank_town();
            return save_town();
        }

        try {
            std::ifstream file(town_file_path, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[LAND] Failed to open town file: %s", town_file_path.string().c_str());
                return false;
            }

            std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if (session.land_proto.ParseFromArray(buffer.data(), buffer.size())) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME, "[LAND] Successfully loaded town file (direct parse)");
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME, "[LAND] Direct parse failed. Attempting Tsto backup offset parse.");

                constexpr size_t backup_offset = 0x0C;
                if (buffer.size() > backup_offset) {
                    if (!session.land_proto.ParseFromArray(buffer.data() + backup_offset, buffer.size() - backup_offset)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[LAND] Failed to parse town file after both direct and backup parse attempts");
                        return false;
                    }
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME, "[LAND] Successfully loaded town file (Tsto Backup)");
                }
                else {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[LAND] Buffer too small for backup format parsing");
                    return false;
                }
            }

            session.land_proto.set_id(session.user_user_id);
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME, "[LAND] Updated land_proto ID to match session: %s", session.user_user_id.c_str());

            return save_town();
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[LAND] Exception while loading town: %s", ex.what());
            return false;
        }
    }


    void Land::create_blank_town() {
        auto& session = tsto::Session::get();

        session.land_proto.Clear();
        auto* friend_data = session.land_proto.mutable_frienddata();
        friend_data->set_dataversion(72);
        friend_data->set_haslemontree(false);
        friend_data->set_language(0);
        friend_data->set_level(0);
        friend_data->set_name("");
        friend_data->set_rating(0);
        friend_data->set_boardwalktilecount(0);

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME, "[LAND] Created new blank town with data version %d", friend_data->dataversion());
    }

    bool Land::save_town() {
        auto& session = tsto::Session::get();

        if (!session.land_proto.IsInitialized()) {
            return false;
        }

        std::filesystem::path towns_dir = "towns";
        std::filesystem::path town_file_path = towns_dir / "mytown.pb";

        std::filesystem::create_directories(towns_dir);

        std::ofstream file(town_file_path, std::ios::binary);
        if (!file.is_open()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PROTOLAND] Failed to open town file for writing: %s",
                town_file_path.string().c_str());
            return false;
        }

        std::string serialized;
        if (!session.land_proto.SerializeToString(&serialized)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PROTOLAND] Failed to serialize town data");
            return false;
        }

        file.write(serialized.data(), serialized.size());
        file.close();

        return true;
    }


    void Land::handle_protoland(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            const std::string uri = ctx->uri();
            const size_t land_start = uri.find("/protoland/") + 11;
            const size_t land_end = uri.find("/", land_start);

            if (land_start == std::string::npos || land_end == std::string::npos || land_end <= land_start) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Invalid URI format: %s", uri.c_str());
                ctx->set_response_http_code(400);
                cb("Invalid URI format");
                return;
            }

            const std::string land_id = uri.substr(land_start, land_end - land_start);


            //seems ios wont load this with this in causes a minidump #lazy fix comment it out
            /*
            // Extract nucleus token
            std::string nucleus_token = ctx->FindRequestHeader("nucleus_token");
            if (nucleus_token.empty()) {
                nucleus_token = ctx->FindRequestHeader("mh_auth_params");
            }

            if (nucleus_token.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Missing nucleus_token");
                const std::string error_xml =
                    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
                    "<error code=\"400\" type=\"MISSING_VALUE\" field=\"nucleus_token\"/>";
                ctx->set_response_http_code(400);
                ctx->AddResponseHeader("Content-Type", "application/xml");
                cb(error_xml);
                return;
            }
            */


            // Determine HTTP method
            const std::string method = ctx->GetMethod();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[PROTOLAND] %s request from %s for land_id: %s",
                method.c_str(), ctx->remote_ip().data(), land_id.c_str());

            if (method == "GET") {
                handle_get_request(ctx, cb, land_id);
            }
            else if (method == "PUT") {
                handle_put_request(ctx, cb);
            }
            else if (method == "POST") {
                handle_post_request(ctx, cb);
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME, "[PROTOLAND] Unsupported method: %s", method.c_str());
                ctx->set_response_http_code(405);
                ctx->AddResponseHeader("Allow", "GET, PUT, POST");
                cb("Method Not Allowed");
            }

        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Exception caught: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("Internal Server Error");
        }
        catch (...) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Unknown error occurred");
            ctx->set_response_http_code(500);
            cb("Internal Server Error");
        }
    }

    void Land::handle_get_request(const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb, const std::string& land_id) {
        if (!load_town()) {
            create_blank_town();
        }

        logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME, "[PROTOLAND] Sending land data for land_id: %s", land_id.c_str());

        headers::set_protobuf_response(ctx);
        std::string serialized;
        tsto::Session::get().land_proto.SerializeToString(&serialized);
        cb(serialized);
    }

    void Land::handle_put_request(const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        const evpp::Slice& body = ctx->body();
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME, "[PROTOLAND] Body size: %zu", body.size());

        if (body.empty()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME, "[PROTOLAND] Creating new empty town");
            create_blank_town();
            if (!save_town()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Failed to save empty town");
                ctx->set_response_http_code(500);
                cb("Failed to save empty town");
                return;
            }
        }
        else {
            auto& session = tsto::Session::get();
            if (!session.land_proto.ParseFromArray(body.data(), body.size())) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Failed to parse request body");
                ctx->set_response_http_code(400);
                cb("Failed to parse body");
                return;
            }

            if (!save_town()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Failed to save town data");
                ctx->set_response_http_code(500);
                cb("Failed to save town data");
                return;
            }
        }

        headers::set_protobuf_response(ctx);
        std::string serialized;
        if (!tsto::Session::get().land_proto.SerializeToString(&serialized)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Failed to serialize response");
            ctx->set_response_http_code(500);
            cb("Failed to serialize response");
            return;
        }
        cb(serialized);
    }

    void Land::handle_post_request(const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        const std::string compressed_data = ctx->body().ToString();
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME, "[PROTOLAND] Received compressed data size: %zu", compressed_data.size());

        std::string decompressed_data;
        const char* encoding = ctx->FindRequestHeader("Content-Encoding");
        if (encoding && strcmp(encoding, "gzip") == 0) {
            decompressed_data = utils::compression::zlib::decompress(compressed_data);
            if (decompressed_data.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Failed to decompress data");
                ctx->set_response_http_code(400);
                cb("Failed to decompress data");
                return;
            }
        }
        else {
            decompressed_data = compressed_data;
        }

        auto& session = tsto::Session::get();
        if (!session.land_proto.ParseFromString(decompressed_data)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Failed to parse decompressed data");
            ctx->set_response_http_code(400);
            cb("Failed to parse data");
            return;
        }

        if (!save_town()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME, "[PROTOLAND] Failed to save land data");
            ctx->set_response_http_code(500);
            cb("Failed to save data");
            return;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME, "[PROTOLAND] Successfully saved land data");

        const std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<WholeLandUpdateResponse/>";

        ctx->AddResponseHeader("Content-Type", "application/xml");
        cb(xml);
    }




    void Land::handle_extraland_update(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();
            size_t land_start = uri.find("/extraLandUpdate/") + 16;
            size_t land_end = uri.find("/protoland/", land_start);
            std::string land_id = uri.substr(land_start, land_end - land_start);

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_LAND, "[EXTRALAND] Request from %s for land_id: %s", ctx->remote_ip().data(), land_id.c_str());


            const std::string method = ctx->GetMethod(); 

            if (method == "POST") {

                const evpp::Slice& body = ctx->body();
                std::string compressed_data(body.data(), body.size());

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND, "[EXTRALAND] Received compressed data size: %zu bytes", compressed_data.size());

                std::string decompressed = utils::compression::zlib::decompress(compressed_data);

                Data::ExtraLandMessage request;
                if (request.ParseFromString(decompressed)) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND, "[EXTRALAND] Successfully parsed request data");

                    if (request.currencydelta_size() > 0) {
                        for (const auto& delta : request.currencydelta()) {
                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND, "[EXTRALAND] Currency Delta - ID: %d, Reason: %s, Amount: %d",
                                delta.id(), delta.reason().c_str(), delta.amount());
                        }
                    }

                    if (request.event_size() > 0) {
                        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND, "[EXTRALAND] Contains %d events", request.event_size());
                    }

                    if (request.pushnotification_size() > 0) {
                        for (const auto& notif : request.pushnotification()) {
                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND, "[EXTRALAND] Push Notification - Template: %s, Message: %s", notif.templatename().c_str(), notif.message().c_str());
                        }
                    }

                    if (request.communitygoaldelta_size() > 0) {
                        for (const auto& goal : request.communitygoaldelta()) {
                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND, "[EXTRALAND] Community Goal Delta - Category: %s, Amount: %lld", goal.category().c_str(), goal.amount());
                        }
                    }
                }
            }

            Data::ExtraLandResponse response;

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_LAND, "[EXTRALAND] Sending response for land_id: %s", land_id.c_str());

            headers::set_protobuf_response(ctx);
            cb(utils::serialization::serialize_protobuf(response));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND, "[EXTRALAND] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }


}