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
            std::string user_id;

            size_t token_start = uri.find("/checkToken/") + 11;
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
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Invalid URI format: %s", uri.c_str());
                ctx->set_response_http_code(400);
                cb("Invalid URI format");
                return;
            }

            const std::string land_id = uri.substr(land_start, land_end - land_start);

            std::string access_token;
            auto auth_header = ctx->FindRequestHeader("mh_auth_params");
            if (auth_header) {
                std::string auth_str(auth_header);
                if (auth_str.substr(0, 7) == "Bearer ") {
                    access_token = auth_str.substr(7);
                }
                else {
                    access_token = auth_str;
                }
            }


            auto& session = tsto::Session::get();

            // Verify mh_uid matches land_id for security
            auto mh_uid = ctx->FindRequestHeader("mh_uid");
            if (mh_uid && std::string(mh_uid) != land_id) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Land ID mismatch - URI: %s, mh_uid: %s",
                    land_id.c_str(), mh_uid);
                ctx->set_response_http_code(403);
                cb("Unauthorized");
                return;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[PROTOLAND] GET request from %s for land_id: %s",
                ctx->remote_ip().c_str(), land_id.c_str());

            const std::string method = ctx->GetMethod();
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
                ctx->set_response_http_code(405);
                ctx->AddResponseHeader("Allow", "GET, PUT, POST");
                cb("Method Not Allowed");
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PROTOLAND] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
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
        try {
            const char* auth_header = ctx->FindRequestHeader("mh_auth_params");
            if (!auth_header) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Missing authentication token");
                ctx->set_response_http_code(401);
                cb("Authentication required");
                return;
            }


            const std::string compressed_data = ctx->body().ToString();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[PROTOLAND] Received compressed data size: %zu", compressed_data.size());

            std::string decompressed_data;
            const char* encoding = ctx->FindRequestHeader("Content-Encoding");
            if (encoding && strcmp(encoding, "gzip") == 0) {
                decompressed_data = utils::compression::zlib::decompress(compressed_data);
                if (decompressed_data.empty()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[PROTOLAND] Failed to decompress data");
                    ctx->set_response_http_code(400);
                    cb("Failed to decompress data");
                    return;
                }
            }
            else {
                decompressed_data = compressed_data;
            }

            auto& session = tsto::Session::get();
            session.access_token = auth_header;

            if (!session.land_proto.ParseFromString(decompressed_data)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Failed to parse decompressed data");
                ctx->set_response_http_code(400);
                cb("Failed to parse data");
                return;
            }

            session.land_proto.set_id(session.user_user_id);

            if (!save_town()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Failed to save land data");
                ctx->set_response_http_code(500);
                cb("Failed to save data");
                return;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[PROTOLAND] Successfully saved land data for user: %s",
                session.user_user_id.c_str());

            const std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
                "<WholeLandUpdateResponse/>";

            ctx->AddResponseHeader("Content-Type", "application/xml");
            cb(xml);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PROTOLAND] Error processing request: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("Internal server error");
        }
    }

    void Land::handle_delete_token(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Received request: RemoteIP: '%s', URI: '%s'",
                std::string(ctx->remote_ip()).c_str(),
                std::string(ctx->uri()).c_str());

            std::string uri = ctx->uri();
            size_t mayhem_id_start = uri.find("/deleteToken/");
            if (mayhem_id_start == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[DELETE TOKEN] Invalid URI format - could not find /deleteToken/");
                headers::set_xml_response(ctx);
                ctx->set_response_http_code(400);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"400\" type=\"BAD_REQUEST\" field=\"Invalid URI format\"/>");
                return;
            }
            mayhem_id_start += 12; 

            size_t mayhem_id_end = uri.find("/protoWholeLandToken/", mayhem_id_start);
            if (mayhem_id_end == std::string::npos) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[DELETE TOKEN] Invalid URI format - could not find /protoWholeLandToken/");
                headers::set_xml_response(ctx);
                ctx->set_response_http_code(400);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"400\" type=\"BAD_REQUEST\" field=\"Invalid URI format\"/>");
                return;
            }

            std::string mayhem_id = uri.substr(mayhem_id_start, mayhem_id_end - mayhem_id_start);
            if (!mayhem_id.empty() && mayhem_id[0] == '/') {
                mayhem_id = mayhem_id.substr(1);
            }
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Extracted mayhem_id: %s", mayhem_id.c_str());

            const char* mh_uid = ctx->FindRequestHeader("mh_uid");
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] mh_uid header: %s", mh_uid ? mh_uid : "not found");

            if (!mh_uid || mayhem_id != mh_uid) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[DELETE TOKEN] mh_uid mismatch or missing. Expected: %s, Got: %s",
                    mayhem_id.c_str(), mh_uid ? mh_uid : "null");
                headers::set_xml_response(ctx);
                ctx->set_response_http_code(404);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"404\" type=\"NOT_FOUND\" field=\"mayhemId\"/>");
                return;
            }

            const char* auth_header = ctx->FindRequestHeader("mh_auth_params");
            if (!auth_header) {
                auth_header = ctx->FindRequestHeader("nucleus_token");
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Auth header present: %s", auth_header ? "yes" : "no");

            if (!auth_header) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[DELETE TOKEN] Missing authentication token");
                headers::set_xml_response(ctx);
                ctx->set_response_http_code(400);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"400\" type=\"MISSING_VALUE\" field=\"nucleus_token\"/>");
                return;
            }

            auto& session = tsto::Session::get();

            const evpp::Slice& body = ctx->body();
            Data::DeleteTokenRequest request;
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Request body size: %zu bytes", body.size());

            if (!request.ParseFromArray(body.data(), body.size())) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[DELETE TOKEN] Failed to parse delete token request");
                headers::set_xml_response(ctx);
                ctx->set_response_http_code(400);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"400\" type=\"BAD_REQUEST\" field=\"Invalid request format\"/>");
                return;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Request token: %s", request.token().c_str());
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Land token: %s", session.land_token.c_str());
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Session key: %s", session.session_key.c_str());

            // Check if tokens match - compare with either land_token or session_key
            if (request.token() != session.land_token && request.token() != session.session_key) {
                // Return result "0" if tokens don't match
                Data::DeleteTokenResponse response;
                response.set_result("0");
                headers::set_protobuf_response(ctx);
                std::string serialized;
                response.SerializeToString(&serialized);
                cb(serialized);
                return;
            }

            // Clear both tokens and return success
            session.land_token.clear();
            session.session_key.clear();
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Successfully deleted token");


            Data::DeleteTokenResponse response;
            response.set_result("1");
            headers::set_protobuf_response(ctx);
            std::string serialized;
            response.SerializeToString(&serialized);
            cb(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Error in delete token request: %s", ex.what());
            headers::set_xml_response(ctx);
            ctx->set_response_http_code(500);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"500\" type=\"INTERNAL_SERVER_ERROR\"/>");
        }
    }

    void Land::handle_extraland_update(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();
            size_t land_start = uri.find("/extraLandUpdate/") + 16;
            size_t land_end = uri.find("/protoland/", land_start);
            std::string land_id = uri.substr(land_start, land_end - land_start);

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_LAND, "[EXTRALAND] Request from %s for land_id: %s", ctx->remote_ip().data(), land_id.c_str());


            const std::string method = ctx->GetMethod(); // 

            if (method == "POST") {

                const evpp::Slice& body = ctx->body();
                std::string compressed_data(body.data(), body.size());

                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND, "[EXTRALAND] Received compressed data size: %zu bytes", compressed_data.size());

                std::string decompressed = utils::compression::zlib::decompress(compressed_data);

                Data::ExtraLandMessage request;
                if (request.ParseFromString(decompressed)) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND, "[EXTRALAND] Successfully parsed request data");

                    // Debug log the contents
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