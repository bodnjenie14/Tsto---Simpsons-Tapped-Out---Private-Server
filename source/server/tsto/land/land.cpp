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
#include <configuration.hpp>

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
        std::string filename = session.town_filename;

        //legacy users or when not logged in
        if (filename.empty()) {
            filename = "mytown.pb";
            session.town_filename = filename;  //set session filename to mytown.pb
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[LAND] Using default town file: %s", filename.c_str());
        }
        //logged in users, validate email format
        else if (filename != "mytown.pb" && (filename.find('@') == std::string::npos || !filename.ends_with(".pb"))) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Invalid town filename format: %s", filename.c_str());
            return false;
        }

        std::filesystem::path town_file_path = "towns/" + filename;

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME, 
            "[LAND] Attempting to load %s", town_file_path.string().c_str());

        //create towns directory if it doesn't exist
        std::filesystem::create_directories("towns");

        //try to load existing town or create new one
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

    bool Land::save_town() {
        auto& session = tsto::Session::get();
        std::string filename = session.town_filename;

        //legacy users or when not logged in, use mytown.pb
        if (filename.empty()) {
            filename = "mytown.pb";
        }

        std::filesystem::path town_file_path = "towns/" + filename;
        
        try {
            std::filesystem::create_directories("towns");

            std::ofstream file(town_file_path, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to open town file for writing: %s", town_file_path.string().c_str());
                return false;
            }

            std::string serialized;
            if (!session.land_proto.SerializeToString(&serialized)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to serialize town data");
                return false;
            }

            file.write(serialized.data(), serialized.size());
            file.close();

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[LAND] Successfully saved town file: %s", town_file_path.string().c_str());
            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Error saving town file: %s", ex.what());
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

        int initial_donuts = std::stoi(utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));
        std::string currency_path = "towns/currency.txt";
        
        std::filesystem::create_directories("towns");
        std::ofstream output(currency_path);
        output << initial_donuts;
        output.close();

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME, 
            "[LAND] Created new blank town with data version %d and initial donuts: %d", 
            friend_data->dataversion(), initial_donuts);
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

            if (request.token() != session.land_token && request.token() != session.session_key) {
                Data::DeleteTokenResponse response;
                response.set_result("0");
                headers::set_protobuf_response(ctx);
                std::string serialized;
                response.SerializeToString(&serialized);
                cb(serialized);
                return;
            }

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


    void Land::handle_extraland_update(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb, const std::string& land_id) {
        try {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[CURRENCY] Processing extraland update for land_id: %s", land_id.c_str());

            std::string body = ctx->body().ToString();
            Data::ExtraLandMessage extraland_msg;
            if (!extraland_msg.ParseFromString(body)) {
                throw std::runtime_error("Failed to parse ExtraLandMessage");
            }

            //grab the current session to access the email-based filename // this cud cause dementia
            auto& session = tsto::Session::get();
            std::string currency_path;
            std::string user_identifier;

            //check if we're using mytown.pb (non-logged in) or email-based town (logged in)
            std::string current_town = session.town_filename.empty() ? "mytown.pb" : session.town_filename;
            
            if (current_town == "mytown.pb") {
                //non logged in user - use currency.txt
                currency_path = "towns/currency.txt";
                user_identifier = "default";
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Using legacy currency file for non-logged in user");
            } else {
                //logged in user - use email-based currency file
                std::string email_base = current_town;
                size_t pb_pos = email_base.find(".pb");
                if (pb_pos != std::string::npos) {
                    email_base = email_base.substr(0, pb_pos);
                }
                size_t txt_pos = email_base.find(".txt");
                if (txt_pos != std::string::npos) {
                    email_base = email_base.substr(0, txt_pos);
                }
                currency_path = "towns/currency_" + email_base + ".txt";
                user_identifier = email_base;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Using email-based currency file for user: %s", email_base.c_str());
            }
            
            std::filesystem::create_directories("towns");
            
            int balance = std::stoi(utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));
            if (std::filesystem::exists(currency_path)) {
                std::ifstream input(currency_path);
                if (input.good()) {
                    input >> balance;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Loaded existing currency data for user: %s (Balance: %d)", 
                        user_identifier.c_str(), balance);
                }
                input.close();
            } else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Creating new currency file for user: %s with initial balance: %d", 
                    user_identifier.c_str(), balance);
            }

            int32_t total_earned = 0;
            int32_t total_spent = 0;
            Data::ExtraLandResponse response;

            for (const auto& delta : extraland_msg.currencydelta()) {
                if (delta.amount() > 0) {
                    total_earned += delta.amount();
                } else {
                    total_spent += std::abs(delta.amount());  //absolute value for spent amount
                }
                
                auto* processed = response.add_processedcurrencydelta();
                processed->set_id(delta.id());
            }

            balance = balance + total_earned - total_spent;

            std::ofstream output(currency_path);
            if (!output.is_open()) {
                throw std::runtime_error("Failed to open currency file for writing: " + currency_path);
            }
            output << balance;
            output.close();

            headers::set_protobuf_response(ctx);
            std::string serialized;
            if (response.SerializeToString(&serialized)) {
                cb(serialized);
            } else {
                throw std::runtime_error("Failed to serialize response");
            }

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_GAME,
                "[CURRENCY] Updated currency for user: %s (Balance: %d, Earned: %d, Spent: %d)",
                user_identifier.c_str(), balance, total_earned, total_spent);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[CURRENCY] Error in extraland update: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }
}