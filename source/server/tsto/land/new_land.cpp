#include <std_include.hpp>
#include "new_land.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <evpp/http/context.h>
#include <evpp/http/http_server.h>
#include <evpp/http/service.h>
#include <3rdparty/libevent/include/event2/http.h>
#include <compression.hpp>
#include <configuration.hpp>
#include "tsto/database/database.hpp"
#include "tsto/statistics/statistics.hpp"
#include "tsto/auth/token_gen.hpp"

namespace tsto::land {

    bool load_town_from_file(const std::string& email, Data::LandMessage& land_proto);
    bool save_town_to_file(const std::string& email, const Data::LandMessage& land_proto);

    bool use_legacy_mode() {
        bool legacy_mode = utils::configuration::ReadBoolean("Land", "UseLegacyMode", false);
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
            "[CONFIG] Legacy mode setting read from configuration: %s", legacy_mode ? "true" : "false");
        return legacy_mode;
    }

    std::string get_default_town_path() {
        std::string default_path = utils::configuration::ReadString("Land", "DefaultTownPath", "towns/mytown.pb");
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
            "[CONFIG] Default town path read from configuration: %s", default_path.c_str());
        return default_path;
    }


    std::string get_town_path_for_user(const std::string& email, const std::string& user_id) {
        bool is_anonymous = (email.find("anonymous_") == 0) || (email.find('@') == std::string::npos);
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
            "[PATH] Determining town path for email: %s, user_id: %s, is_anonymous: %s",
            email.c_str(), user_id.c_str(), is_anonymous ? "true" : "false");

        std::string result;
        if (is_anonymous) {
            bool legacy = use_legacy_mode();
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PATH] Anonymous user detected, legacy_mode: %s", legacy ? "true" : "false");

            if (legacy) {
                result = get_default_town_path();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                    "[PATH] Using legacy mode default town path: %s", result.c_str());
            }
            else {
                result = "towns/anon_" + user_id + ".pb";
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                    "[PATH] Using modern mode unique town path: %s", result.c_str());
            }
        }
        else {
            result = "towns/" + email + ".pb";
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PATH] Using email-based town path for registered user: %s", result.c_str());
        }

        return result;
    }

    bool extract_user_info(const evpp::http::ContextPtr& ctx,
        std::string& email,
        std::string& user_id,
        std::string& mayhem_id,
        std::string& access_token) {
        auto& db = tsto::database::Database::get_instance();
        bool user_found = false;

        std::string uri = ctx->uri();
        size_t mayhem_id_start = uri.find("/protoland/");
        if (mayhem_id_start != std::string::npos) {
            mayhem_id_start += 11;   
            size_t mayhem_id_end = uri.find("/", mayhem_id_start);
            if (mayhem_id_end != std::string::npos) {
                mayhem_id = uri.substr(mayhem_id_start, mayhem_id_end - mayhem_id_start);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                    "[LAND] Found mayhem_id in URL: %s", mayhem_id.c_str());
                if (db.get_email_by_mayhem_id(mayhem_id, email) &&
                    db.get_user_id(email, user_id)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                        "[LAND] Found user by mayhem ID from URL: %s -> %s",
                        mayhem_id.c_str(), email.c_str());
                    user_found = true;
                }
            }
        }

        if (!user_found) {
            const char* mh_uid_header = ctx->FindRequestHeader("mh_uid");
            if (mh_uid_header) {
                std::string requested_mayhem_id = mh_uid_header;
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                    "[LAND] Found mh_uid header (mayhem_id): %s", requested_mayhem_id.c_str());

                if (db.get_email_by_mayhem_id(requested_mayhem_id, email)) {
                    mayhem_id = requested_mayhem_id;
                    if (db.get_user_id(email, user_id)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                            "[LAND] Found user by mayhem ID from mh_uid header: %s -> %s",
                            requested_mayhem_id.c_str(), email.c_str());
                        user_found = true;
                    }
                }
            }
        }

        if (!user_found) {
            const char* auth_header = ctx->FindRequestHeader("nucleus_token");
            if (!auth_header) {
                auth_header = ctx->FindRequestHeader("mh_auth_params");
                if (auth_header) {
                    std::string auth_str(auth_header);
                    if (auth_str.substr(0, 7) == "Bearer ") {
                        access_token = auth_str.substr(7);
                    }
                    else {
                        access_token = auth_str;
                    }
                }
            }
            else {
                access_token = auth_header;
            }

            if (access_token.empty()) {
                auth_header = ctx->FindRequestHeader("access_token");
                if (auth_header) {
                    access_token = auth_header;
                }
            }

            if (!access_token.empty()) {
                if (db.get_email_by_token(access_token, email)) {
                    if (db.get_user_id(email, user_id) && db.get_mayhem_id(email, mayhem_id)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                            "[LAND] Found user by access token: %s -> %s (mayhem_id: %s)",
                            access_token.c_str(), email.c_str(), mayhem_id.c_str());
                        user_found = true;
                    }
                }
            }
        }

        if (user_found) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[LAND] User identified: email=%s, user_id=%s, mayhem_id=%s",
                email.c_str(), user_id.c_str(), mayhem_id.c_str());
        }
        else {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                "[LAND] Failed to identify user from request");
        }

        return user_found;
    }

    void Land::handle_proto_whole_land_token(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();
            std::string token;
            std::string email;
            std::string user_id;
            std::string mayhem_id;
            std::string access_token;

            if (extract_user_info(ctx, email, user_id, mayhem_id, access_token)) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                    "[CHECK TOKEN] Found user: email=%s, user_id=%s, mayhem_id=%s",
                    email.c_str(), user_id.c_str(), mayhem_id.c_str());
            }

            size_t token_start = uri.find("/checkToken/");
            if (token_start != std::string::npos) {
                token_start += 11;    
                size_t token_end = uri.find("/", token_start);
                if (token_end != std::string::npos) {
                    token = uri.substr(token_start, token_end - token_start);
                }
            }

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_LAND,
                "[CHECK TOKEN] Request from %s: %s", ctx->remote_ip().data(), ctx->uri().data());

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[CHECK TOKEN] Request params:\n"
                "URI: %s\n"
                "Token: %s",
                ctx->uri().c_str(),
                token.c_str()
            );

            auto& db = tsto::database::Database::get_instance();

            std::string session_key;
            if (!db.get_session_key(email, session_key)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[CHECK TOKEN] Failed to get session key for email: %s", email.c_str());
            }
            else if (session_key.empty()) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                    "[CHECK TOKEN] Empty session key found in database for email: %s", email.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                    "[CHECK TOKEN] Using session key from database: %s for email: %s",
                    session_key.c_str(), email.c_str());
            }

            Data::TokenData response;
            response.set_sessionkey(session_key);
            response.set_expirationdate(0);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_LAND,
                "[CHECK TOKEN] Sending response with session key: %s", session_key.c_str());

            Data::LandMessage land_proto;
            if (load_town_from_file(email, land_proto)) {
                std::string serialized_data;
                if (land_proto.SerializeToString(&serialized_data)) {
                    std::string compressed_data = utils::compression::zlib::compress(serialized_data);

                    ctx->set_response_http_code(200);
                    ctx->AddResponseHeader("Content-Type", "application/x-protobuf");
                    ctx->AddResponseHeader("Content-Encoding", "deflate");

                    cb(compressed_data);
                    return;
                }
            }

            headers::set_protobuf_response(ctx);

            std::string serialized;
            if (response.SerializeToString(&serialized)) {
                cb(serialized);
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[CHECK TOKEN] Failed to serialize response");
                ctx->set_response_http_code(500);
                cb("");
            }
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[CHECK TOKEN] Exception in handle_proto_whole_land_token: %s", e.what());
            ctx->set_response_http_code(500);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"500\" type=\"INTERNAL_SERVER_ERROR\" field=\"Internal server error\"/>");
        }
    }


    bool load_town_from_file(const std::string& email, Data::LandMessage& land_proto) {
        std::string user_id;
        std::string mayhem_id;
        std::string land_save_path;
        auto& db = tsto::database::Database::get_instance();

        if (!db.get_mayhem_id(email, mayhem_id) || mayhem_id.empty()) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to get mayhem_id for email: %s", email.c_str());
        }
        else {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Found mayhem_id for user: %s", mayhem_id.c_str());
        }

        if (!db.get_user_id(email, user_id)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to get user_id for email: %s", email.c_str());
            return false;
        }

        if (db.get_land_save_path(email, land_save_path) && !land_save_path.empty()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Using land save path from database: %s", land_save_path.c_str());
        }
        else {

            land_save_path = get_town_path_for_user(email, user_id);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Using standardized town path from helper function: %s", land_save_path.c_str());

            if (!db.update_land_save_path(email, land_save_path)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Failed to update land save path in database");
            }
            else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Updated land save path in database: %s", land_save_path.c_str());
            }
        }

        std::filesystem::path dir_path = std::filesystem::path(land_save_path).parent_path();
        if (!std::filesystem::exists(dir_path)) {
            std::filesystem::create_directories(dir_path);
        }

        if (!std::filesystem::exists(land_save_path)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Town file does not exist: %s", land_save_path.c_str());
            return false;
        }

        std::ifstream file(land_save_path, std::ios::binary);
        if (!file.is_open()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to open town file: %s", land_save_path.c_str());
            return false;
        }

        std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        file.close();

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
            "[PROTOLAND] Town file size: %zu bytes", buffer.size());

        bool parsed = false;

        if (land_proto.ParseFromArray(buffer.data(), static_cast<int>(buffer.size()))) {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Successfully loaded town file (direct parse)");
            parsed = true;
        }
        else {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Direct parse failed. Attempting alternative parsing methods.");

            constexpr size_t backup_offset = 0x0C;    
            if (buffer.size() > backup_offset) {
                if (land_proto.ParseFromArray(buffer.data() + backup_offset,
                    static_cast<int>(buffer.size() - backup_offset))) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Successfully loaded town file (12-byte offset)");
                    parsed = true;
                }
            }
        }

        if (!parsed) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to parse town data from file: %s", land_save_path.c_str());
            return false;
        }

        if (!mayhem_id.empty() && land_proto.id() != mayhem_id) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Updating land ID from %s to %s",
                land_proto.id().c_str(), mayhem_id.c_str());
            land_proto.set_id(mayhem_id);

            std::string serialized;
            if (land_proto.SerializeToString(&serialized)) {
                std::ofstream outfile(land_save_path, std::ios::binary);
                if (outfile.is_open()) {
                    outfile.write(serialized.data(), serialized.size());
                    outfile.close();
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Updated town file with correct mayhem_id");
                }
            }
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
            "[PROTOLAND] Successfully loaded town for email: %s with mayhem_id: %s",
            email.c_str(), mayhem_id.c_str());
        return true;
    }

    bool save_town_to_file(const std::string& email, const Data::LandMessage& land_proto) {
        std::string user_id;
        std::string mayhem_id;
        std::string land_save_path;
        auto& db = tsto::database::Database::get_instance();

        if (!db.get_mayhem_id(email, mayhem_id) || mayhem_id.empty()) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to get mayhem_id for email: %s", email.c_str());
        }
        else {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Found mayhem_id for user: %s", mayhem_id.c_str());

            if (land_proto.id() != mayhem_id) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Land proto ID (%s) doesn't match mayhem_id (%s)",
                    land_proto.id().c_str(), mayhem_id.c_str());
            }
        }

        if (!db.get_user_id(email, user_id)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to get user_id for email: %s", email.c_str());
            return false;
        }

        if (db.get_land_save_path(email, land_save_path) && !land_save_path.empty()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Using land save path from database: %s", land_save_path.c_str());
        }
        else {
            land_save_path = get_town_path_for_user(email, user_id);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Using standardized town path from helper function: %s", land_save_path.c_str());

            if (!db.update_land_save_path(email, land_save_path)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Failed to update land save path in database");
            }
            else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Updated land save path in database: %s", land_save_path.c_str());
            }
        }

        if (land_save_path.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Empty town path generated for email: %s", email.c_str());
            return false;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
            "[PROTOLAND] Saving town to path: %s", land_save_path.c_str());

        std::filesystem::path cwd = std::filesystem::current_path();
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
            "[PROTOLAND] Current working directory: %s", cwd.string().c_str());

        std::filesystem::path towns_dir("towns");
        if (!std::filesystem::exists(towns_dir)) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Creating main towns directory: %s", towns_dir.string().c_str());
            try {
                bool created = std::filesystem::create_directory(towns_dir);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Main towns directory creation result: %s", created ? "success" : "failed");

                if (!created || !std::filesystem::exists(towns_dir)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Failed to create main towns directory: %s",
                        towns_dir.string().c_str());
                    return false;
                }
            }
            catch (const std::filesystem::filesystem_error& e) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Exception creating main towns directory: %s, error: %s",
                    towns_dir.string().c_str(), e.what());
                return false;
            }
        }

        std::filesystem::path dir_path = std::filesystem::path(land_save_path).parent_path();
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
            "[PROTOLAND] Town directory path: %s, exists: %s",
            dir_path.string().c_str(), std::filesystem::exists(dir_path) ? "true" : "false");

        if (!std::filesystem::exists(dir_path)) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Creating directory: %s", dir_path.string().c_str());
            try {
                bool created = std::filesystem::create_directories(dir_path);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Directory creation result: %s", created ? "success" : "failed");

                if (!created || !std::filesystem::exists(dir_path)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Directory still doesn't exist after creation attempt: %s",
                        dir_path.string().c_str());
                    return false;
                }
            }
            catch (const std::filesystem::filesystem_error& e) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Failed to create directory: %s, error: %s",
                    dir_path.string().c_str(), e.what());
                return false;
            }
        }

        std::string serialized_data;
        if (!land_proto.SerializeToString(&serialized_data)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to serialize town data for email: %s, path: %s",
                email.c_str(), land_save_path.c_str());
            return false;
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
            "[PROTOLAND] Serialized data size: %zu bytes", serialized_data.size());

        try {
            std::string temp_path = land_save_path + ".tmp";
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Testing write permissions with temp file: %s", temp_path.c_str());

            std::ofstream test_file(temp_path, std::ios::binary);
            if (!test_file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Failed to create test file: %s", temp_path.c_str());

                try {
                    std::error_code ec;
                    auto perms = std::filesystem::status(dir_path, ec).permissions();
                    bool is_writable = std::filesystem::is_directory(dir_path, ec) &&
                        ((perms & std::filesystem::perms::owner_write) != std::filesystem::perms::none);

                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Directory %s is writable: %s, error: %s",
                        dir_path.string().c_str(),
                        is_writable ? "true" : "false",
                        ec.message().c_str());
                }
                catch (const std::exception& e) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Error checking directory permissions: %s", e.what());
                }

                return false;
            }

            test_file.close();
            std::filesystem::remove(temp_path);

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Writing to town file: %s", land_save_path.c_str());

            std::ofstream file(land_save_path, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to open town file for writing: %s", land_save_path.c_str());
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Failed to open town file for writing: %s", land_save_path.c_str());
                return false;
            }

            const size_t BUFFER_SIZE = 8192;   
            const char* data = serialized_data.data();
            size_t remaining = serialized_data.size();

            while (remaining > 0) {
                size_t chunk_size = std::min(remaining, BUFFER_SIZE);
                file.write(data, chunk_size);
                if (file.fail()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Failed while writing to file: %s", land_save_path.c_str());
                    file.close();
                    return false;
                }
                data += chunk_size;
                remaining -= chunk_size;
            }

            file.close();

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Successfully saved town for email: %s, mayhem_id: %s (size: %zu bytes)",
                email.c_str(), mayhem_id.c_str(), serialized_data.size());
            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Exception while saving town: %s", ex.what());
            return false;
        }
    }

    void Land::handle_protoland(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();
            std::string land_id;
            std::string email;
            std::string user_id;
            std::string mayhem_id;
            std::string access_token;

            if (extract_user_info(ctx, email, user_id, mayhem_id, access_token)) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Found user: email=%s, user_id=%s, mayhem_id=%s",
                    email.c_str(), user_id.c_str(), mayhem_id.c_str());
            }

            size_t land_id_start = uri.find("/protoland/");
            if (land_id_start != std::string::npos) {
                land_id_start += 11;   
                size_t land_id_end = uri.find("/", land_id_start);
                if (land_id_end != std::string::npos) {
                    land_id = uri.substr(land_id_start, land_id_end - land_id_start);
                }
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Handling request for land ID: %s, method: %s",
                land_id.c_str(), ctx->GetMethod().c_str());

            std::string http_method = ctx->GetMethod();
            if (http_method == "GET") {
                handle_get_request(ctx, cb, land_id);
            }
            else if (http_method == "PUT") {
                handle_put_request(ctx, cb);
            }
            else if (http_method == "POST") {
                handle_post_request(ctx, cb);
            }
            else {
                ctx->set_response_http_code(405);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"405\" type=\"METHOD_NOT_ALLOWED\" field=\"Method not allowed\"/>");
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
        auto& db = tsto::database::Database::get_instance();
        std::string email, user_id, mayhem_id, access_token;
        bool user_found = extract_user_info(ctx, email, user_id, mayhem_id, access_token);

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
            "[PROTOLAND] GET request - User found: %d, Email: %s, User ID: %s, Mayhem ID: %s",
            user_found, email.c_str(), user_id.c_str(), mayhem_id.c_str());

        if (!user_found) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] User not found in GET request");
            ctx->set_response_http_code(401);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"401\" type=\"UNAUTHORIZED\" field=\"user_identification\"/>");
            return;
        }

        bool mayhem_id_mismatch = !land_id.empty() && !mayhem_id.empty() && land_id != mayhem_id;
        if (mayhem_id_mismatch) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Mayhem ID mismatch - URL: %s, User: %s - Using user's mayhem_id",
                land_id.c_str(), mayhem_id.c_str());

            std::string other_email;
            if (db.get_email_by_mayhem_id(land_id, other_email) && !other_email.empty() && other_email != email) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Attempted to access another user's town: URL mayhem_id %s belongs to %s",
                    land_id.c_str(), other_email.c_str());
                ctx->set_response_http_code(403);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"403\" type=\"FORBIDDEN\" field=\"mayhem_id\"/>");
                return;
            }
        }

        if (access_token.empty()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] No access token provided in GET request for user: %s (continuing anyway)", email.c_str());
        }

        std::string land_save_path;
        if (!db.get_land_save_path(email, land_save_path) || land_save_path.empty()) {
            land_save_path = get_town_path_for_user(email, user_id);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Using standardized town path from helper function: %s", land_save_path.c_str());

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[PROTOLAND] Setting new database land save path for GET request: %s: %s",
                email.c_str(), land_save_path.c_str());

            if (!db.update_land_save_path(email, land_save_path)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Failed to update land save path in database for GET request: %s", email.c_str());
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Using existing land save path from database: %s", land_save_path.c_str());
        }

        Data::LandMessage land_proto;
        bool town_loaded = load_town_from_file(email, land_proto);

        if (!town_loaded) {
            land_proto.Clear();

            if (!mayhem_id.empty()) {
                land_proto.set_id(mayhem_id);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Updated land proto ID to match mayhem_id: %s", mayhem_id.c_str());
            }
            else {
                std::string default_id = "default_" + std::to_string(std::time(nullptr));
                land_proto.set_id(default_id);
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Using generated default ID: %s", default_id.c_str());
            }

            auto* friend_data = land_proto.mutable_frienddata();
            friend_data->set_dataversion(72);
            friend_data->set_haslemontree(false);
            friend_data->set_language(0);
            friend_data->set_level(0);
            friend_data->set_name("");
            friend_data->set_rating(0);
            friend_data->set_boardwalktilecount(0);

            if (save_town_to_file(email, land_proto)) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Created and saved blank town for user: email=%s, mayhem_id=%s",
                    email.c_str(), mayhem_id.c_str());
            }
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
            "[PROTOLAND] Sending land data for user: email=%s, mayhem_id=%s",
            email.c_str(), mayhem_id.c_str());

        headers::set_protobuf_response(ctx);
        std::string serialized;
        land_proto.SerializeToString(&serialized);
        cb(serialized);
    }

    void Land::handle_put_request(const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        auto& db = tsto::database::Database::get_instance();
        std::string email, user_id, mayhem_id, access_token;
        bool user_found = extract_user_info(ctx, email, user_id, mayhem_id, access_token);

        if (!user_found || email.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] No valid user found for PUT request");
            ctx->set_response_http_code(401);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"401\" type=\"UNAUTHORIZED\" field=\"No valid user found\"/>");
            return;
        }

        if (!db.update_access_token(email, access_token)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to update access token in database for PUT request: %s", email.c_str());
            ctx->set_response_http_code(500);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"500\" type=\"INTERNAL_SERVER_ERROR\" field=\"Failed to update access token in database\"/>");
            return;
        }

        std::string body = ctx->body().ToString();
        if (body.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Empty request body in PUT request");
            ctx->set_response_http_code(400);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"400\" type=\"BAD_REQUEST\" field=\"Empty request body\"/>");
            return;
        }

        Data::LandMessage land_proto;
        if (!land_proto.ParseFromString(body)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to parse protobuf message in PUT request");
            ctx->set_response_http_code(400);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"400\" type=\"BAD_REQUEST\" field=\"Invalid protobuf message\"/>");
            return;
        }

        if (!validate_land_data(land_proto)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Invalid land data in PUT request");
            ctx->set_response_http_code(400);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"400\" type=\"BAD_REQUEST\" field=\"Invalid land data\"/>");
            return;
        }

        if (!save_town_to_file(email, land_proto)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Failed to save land data in PUT request");
            ctx->set_response_http_code(500);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"500\" type=\"INTERNAL_SERVER_ERROR\" field=\"Failed to save land data\"/>");
            return;
        }

        headers::set_protobuf_response(ctx);
        std::string serialized;
        if (!land_proto.SerializeToString(&serialized)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[PROTOLAND] Failed to serialize response");
            ctx->set_response_http_code(500);
            cb("Failed to serialize response");
            return;
        }
        cb(serialized);
    }

    bool Land::validate_land_data(const Data::LandMessage& land_data) {
        if (land_data.id().empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Invalid land data: missing ID");
            return false;
        }

        if (!land_data.has_frienddata()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Invalid land data: missing friend data");
            return false;
        }

        return true;
    }

    void Land::handle_post_request(const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            auto& db = tsto::database::Database::get_instance();
            
            bool transaction_started = db.begin_transaction();
            if (!transaction_started) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Failed to start transaction for POST request");
            } else {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Started transaction for POST request");
            }
            
            std::string email;
            std::string user_id;
            std::string mayhem_id;
            std::string access_token;
            bool user_found = false;

            std::string uri = ctx->uri();
            const size_t land_start = uri.find("/protoland/") + 11;
            const size_t land_end = uri.find("/", land_start);

            if (land_start != std::string::npos && land_end != std::string::npos && land_end > land_start) {
                std::string mayhem_id_from_url = uri.substr(land_start, land_end - land_start);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Attempting to match user by mayhem ID from URL: %s", mayhem_id_from_url.c_str());

                if (db.get_email_by_mayhem_id(mayhem_id_from_url, email)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[PROTOLAND] Found user by mayhem ID from URL in POST request: %s -> %s",
                        mayhem_id_from_url.c_str(), email.c_str());
                    mayhem_id = mayhem_id_from_url;
                    db.get_user_id(email, user_id);
                    user_found = true;
                }
            }

            if (!user_found) {
                user_found = extract_user_info(ctx, email, user_id, mayhem_id, access_token);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] POST request - User found via extract_user_info: %d, Email: %s, User ID: %s, Mayhem ID: %s",
                    user_found, email.c_str(), user_id.c_str(), mayhem_id.c_str());
            }

            if (!user_found) {
                const char* mh_uid_header = ctx->FindRequestHeader("mh_uid");
                if (mh_uid_header) {
                    std::string requested_user_id = mh_uid_header;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[PROTOLAND] Found mh_uid header: %s", requested_user_id.c_str());

                    std::string user_id_email;
                    if (db.get_email_by_user_id(requested_user_id, user_id_email)) {
                        email = user_id_email;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[PROTOLAND] Found user by user ID from mh_uid header in POST request: %s -> %s",
                            requested_user_id.c_str(), email.c_str());
                        user_found = true;
                    }

                    if (!user_found) {
                        std::string mayhem_email;
                        if (db.get_email_by_mayhem_id(requested_user_id, mayhem_email)) {
                            email = mayhem_email;
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                                "[PROTOLAND] Found user by mayhem ID from mh_uid header in POST request: %s -> %s",
                                requested_user_id.c_str(), email.c_str());
                            user_found = true;
                        }
                    }
                }
            }

            if (!user_found) {
                const char* auth_header = ctx->FindRequestHeader("mh_auth_params");
                if (!auth_header) {
                    auth_header = ctx->FindRequestHeader("nucleus_token");
                }

                if (auth_header) {
                    access_token = auth_header;
                    if (db.get_email_by_token(access_token, email)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[PROTOLAND] Found user by access token in POST request: %s", email.c_str());
                        user_found = true;
                    }
                }
            }

            if (!user_found) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] No valid user found for POST request");
                
                if (transaction_started) {
                    db.rollback_transaction();
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Transaction rolled back due to no valid user found");
                }
                
                ctx->set_response_http_code(403);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"403\" type=\"BAD_REQUEST\" field=\"Invalid AccessToken, User ID, or Mayhem ID\"/>");
                return;
            }

            if (!db.get_user_id(email, user_id)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] No user_id found for email in POST request: %s", email.c_str());
                if (transaction_started) {
                    db.rollback_transaction();
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Transaction rolled back due to no user_id found");
                }
                ctx->set_response_http_code(404);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"404\" type=\"NOT_FOUND\" field=\"user_id\"/>");
                return;
            }


            std::string land_save_path;
            if (!db.get_land_save_path(email, land_save_path) || land_save_path.empty()) {
                land_save_path = get_town_path_for_user(email, user_id);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Using standardized town path from helper function: %s", land_save_path.c_str());

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Setting new database land save path for POST request: %s: %s",
                    email.c_str(), land_save_path.c_str());

                if (!db.update_land_save_path(email, land_save_path)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[PROTOLAND] Failed to update land save path in database for POST request: %s", email.c_str());
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Using database land save path for POST request: %s: %s",
                    email.c_str(), land_save_path.c_str());
            }

            std::string filename = land_save_path.substr(land_save_path.find_last_of("/\\") + 1);

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                "[PROTOLAND] Using town filename from database: %s", filename.c_str());

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

            Data::LandMessage land_proto;
            if (!land_proto.ParseFromString(decompressed_data)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Failed to parse decompressed data");
                ctx->set_response_http_code(400);
                cb("Failed to parse data");
                return;
            }

            if (!mayhem_id.empty()) {
                land_proto.set_id(mayhem_id);
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                    "[PROTOLAND] Updated land proto ID to match mayhem_id: %s", mayhem_id.c_str());
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Saving town data using standalone function for email: %s, path: %s",
                email.c_str(), land_save_path.c_str());

            if (!save_town_to_file(email, land_proto)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[PROTOLAND] Failed to save land data for email: %s, path: %s",
                    email.c_str(), land_save_path.c_str());
                    
                if (transaction_started) {
                    db.rollback_transaction();
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Transaction rolled back due to failure to save town");
                }
                
                ctx->set_response_http_code(500);
                cb("Failed to save data");
                return;
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[PROTOLAND] Successfully saved land data for user: %s",
                user_id.c_str());
                
            if (transaction_started) {
                if (!db.commit_transaction()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Failed to commit transaction in POST request");
                    db.rollback_transaction();
                } else {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                        "[PROTOLAND] Transaction committed successfully in POST request");
                }
            }

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
            std::string remote_ip = std::string(ctx->remote_ip());
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Received request: RemoteIP: '%s', URI: '%s'",
                remote_ip.c_str(),
                std::string(ctx->uri()).c_str());

            std::string email, user_id, mayhem_id, access_token;
            auto& db = tsto::database::Database::get_instance();
            bool user_found = false;

            std::string uri = ctx->uri();
            size_t mayhem_id_start = uri.find("/deleteToken/");
            if (mayhem_id_start != std::string::npos) {
                mayhem_id_start += 12;   
                size_t mayhem_id_end = uri.find("/", mayhem_id_start);
                if (mayhem_id_end != std::string::npos) {
                    std::string url_mayhem_id = uri.substr(mayhem_id_start, mayhem_id_end - mayhem_id_start);
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                        "[DELETE TOKEN] Found mayhem_id in URL: %s", url_mayhem_id.c_str());

                    if (db.get_email_by_mayhem_id(url_mayhem_id, email) &&
                        db.get_user_id(email, user_id)) {
                        mayhem_id = url_mayhem_id;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                            "[DELETE TOKEN] Found user by mayhem ID from URL: %s -> %s",
                            mayhem_id.c_str(), email.c_str());
                        user_found = true;
                    }
                }
            }

            if (!user_found) {
                const char* mh_uid_header = ctx->FindRequestHeader("mh_uid");
                if (mh_uid_header) {
                    std::string header_mayhem_id = mh_uid_header;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                        "[DELETE TOKEN] Found mh_uid header (mayhem_id): %s", header_mayhem_id.c_str());

                    if (db.get_email_by_mayhem_id(header_mayhem_id, email)) {
                        mayhem_id = header_mayhem_id;
                        if (db.get_user_id(email, user_id)) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                                "[DELETE TOKEN] Found user by mayhem ID from mh_uid header: %s -> %s",
                                header_mayhem_id.c_str(), email.c_str());
                            user_found = true;
                        }
                    }
                }
            }

            if (!user_found) {
                const char* auth_header = ctx->FindRequestHeader("mh_auth_params");
                if (!auth_header) {
                    auth_header = ctx->FindRequestHeader("nucleus_token");
                }

                if (auth_header) {
                    access_token = auth_header;
                    if (db.get_email_by_token(access_token, email)) {
                        if (db.get_user_id(email, user_id) && db.get_mayhem_id(email, mayhem_id)) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                                "[DELETE TOKEN] Found user by access token: %s -> %s (mayhem_id: %s)",
                                access_token.c_str(), email.c_str(), mayhem_id.c_str());
                            user_found = true;
                        }
                    }
                }
            }

            if (!user_found) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[DELETE TOKEN] Failed to identify user from request");
                headers::set_xml_response(ctx);
                ctx->set_response_http_code(401);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"401\" type=\"UNAUTHORIZED\" field=\"user_identification\"/>");
                return;
            }

            const char* mh_uid = ctx->FindRequestHeader("mh_uid");
            if (mh_uid) {
                std::string mh_uid_str = mh_uid;
                if (mh_uid_str != mayhem_id) {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                        "[DELETE TOKEN] mh_uid header doesn't match identified mayhem_id. Header: %s, Identified: %s",
                        mh_uid_str.c_str(), mayhem_id.c_str());
                }
            }

            if (access_token.empty()) {
                const char* auth_header = ctx->FindRequestHeader("mh_auth_params");
                if (!auth_header) {
                    auth_header = ctx->FindRequestHeader("nucleus_token");
                }

                if (auth_header) {
                    access_token = auth_header;
                }
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Auth token present: %s", !access_token.empty() ? "yes" : "no");

            if (access_token.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[DELETE TOKEN] Missing authentication token");
                headers::set_xml_response(ctx);
                ctx->set_response_http_code(400);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"400\" type=\"MISSING_VALUE\" field=\"nucleus_token\"/>");
                return;
            }

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

            if (request.token() != access_token) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_LAND,
                    "[DELETE TOKEN] Request token doesn't match access token");
                Data::DeleteTokenResponse response;
                response.set_result("0");
                headers::set_protobuf_response(ctx);
                std::string serialized;
                response.SerializeToString(&serialized);
                cb(serialized);
                return;
            }

            if (!db.update_access_token(email, "")) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_LAND,
                    "[DELETE TOKEN] Failed to clear token for email: %s", email.c_str());
                headers::set_xml_response(ctx);
                ctx->set_response_http_code(500);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"500\" type=\"INTERNAL_SERVER_ERROR\"/>");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_LAND,
                "[DELETE TOKEN] Successfully deleted token for user: email=%s, mayhem_id=%s",
                email.c_str(), mayhem_id.c_str());

            tsto::statistics::Statistics::get_instance().unregister_client_ip(remote_ip);

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

            std::string email;
            std::string user_id;
            std::string mayhem_id;
            std::string access_token;

            if (!land_id.empty()) {
                mayhem_id = land_id;
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Using land_id as mayhem_id hint: %s", mayhem_id.c_str());
            }

            bool user_found = extract_user_info(ctx, email, user_id, mayhem_id, access_token);

            if (!user_found) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[CURRENCY] No valid user found for extraland update");
                ctx->set_response_http_code(403);
                cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"403\" type=\"BAD_REQUEST\" field=\"Invalid AccessToken, User ID, or Mayhem ID\"/>");
                return;
            }

            auto& db = tsto::database::Database::get_instance();
            if (user_id.empty()) {
                if (!db.get_user_id(email, user_id)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[CURRENCY] No user_id found for email: %s", email.c_str());
                    ctx->set_response_http_code(404);
                    cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<error code=\"404\" type=\"NOT_FOUND\" field=\"user_id\"/>");
                    return;
                }
            }


            bool use_legacy_mode = utils::configuration::ReadBoolean("Land", "UseLegacyMode", false);
            bool user_is_anonymous = email.find('@') == std::string::npos;

            std::string currency_path;
            std::string user_identifier;

            if (user_is_anonymous) {
                if (use_legacy_mode) {
                    currency_path = "towns/mytown.txt";
                    user_identifier = "legacy_anonymous";
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Anonymous user detected, using legacy shared filename: %s",
                        currency_path.c_str());
                }
                else {
                    currency_path = "towns/anon_" + user_id + ".txt";
                    user_identifier = "anon_" + user_id;
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_GAME,
                        "[CURRENCY] Anonymous user detected, using unique filename: %s",
                        currency_path.c_str());
                }
            }
            else {
                currency_path = "towns/" + email + ".txt";
                user_identifier = email;
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
            }
            else {
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
                }
                else {
                    total_spent += std::abs(delta.amount());      
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
            }
            else {
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

    bool Land::load_town_by_email(const std::string& email) {
        auto& db = tsto::database::Database::get_instance();
        Data::LandMessage land_proto;
        std::string filename;

        if (email.empty()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] No valid user found, defaulting to mytown.pb");
            filename = "mytown.pb";

            return load_town_from_file("default", land_proto);
        }

        std::string land_save_path;
        if (db.get_land_save_path(email, land_save_path) && !land_save_path.empty()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Using database land save path for %s: %s",
                email.c_str(), land_save_path.c_str());

            filename = land_save_path.substr(land_save_path.find_last_of("/\\") + 1);
        }
        else {
            filename = email + ".pb";
            land_save_path = "towns/" + filename;

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Setting new database land save path for %s: %s",
                email.c_str(), land_save_path.c_str());

            if (db.update_land_save_path(email, land_save_path)) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[LAND] Successfully updated land save path in database for %s", email.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to update land save path in database for %s", email.c_str());
            }
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[LAND] Loading town for email: %s (filename: %s)", email.c_str(), filename.c_str());

        return load_town_from_file(email, land_proto);
    }

    bool Land::save_town_as(const std::string& email) {
        try {
            if (email.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Cannot save town with empty email");
                return false;
            }

            auto& db = tsto::database::Database::get_instance();

            thread_local static Data::LandMessage land_proto;

            std::string filename = "towns/" + email + ".pb";
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Saving town as: %s", filename.c_str());

            std::filesystem::create_directories("towns");

            std::string mayhem_id;
            if (!email.empty() && db.get_mayhem_id(email, mayhem_id) && !mayhem_id.empty()) {
                land_proto.set_id(mayhem_id);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[LAND] Setting mayhem_id in land proto: %s", mayhem_id.c_str());
            }
            else {
                if (land_proto.id().empty()) {
                    std::string default_id = "90159726165211658982621159447878257465";
                    land_proto.set_id(default_id);
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[LAND] Set default ID for town: %s", default_id.c_str());
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[LAND] Preserved existing ID for legacy town: %s", land_proto.id().c_str());
                }
            }

            return save_town_to_file(email, land_proto);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Error saving town: %s", ex.what());
            return false;
        }
    }

    bool Land::copy_town(const std::string& source_email, const std::string& target_email) {
        if (source_email.empty() || target_email.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Cannot copy town - source or target email is empty");
            return false;
        }

        if (source_email == target_email) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Source and target emails are the same, no need to copy");
            return true;
        }

        Data::LandMessage land_proto;

        if (source_email.empty()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] No valid source email, defaulting to mytown.pb");

            if (!load_town_from_file("default", land_proto)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to load default town");
                return false;
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Loading town for email: %s", source_email.c_str());

            if (!load_town_from_file(source_email, land_proto)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to load source town: %s", source_email.c_str());
                return false;
            }
        }

        bool result = save_town_to_file(target_email, land_proto);

        if (result) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Successfully copied town from %s to %s",
                source_email.c_str(), target_email.c_str());

            auto& db = tsto::database::Database::get_instance();
            std::string target_path = "towns/" + target_email + ".pb";
            if (!db.update_land_save_path(target_email, target_path)) {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to update land save path in database for %s", target_email.c_str());
            }
        }
        else {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Failed to save target town: %s", target_email.c_str());
        }

        return result;
    }

    bool Land::import_town_file(const std::string& source_path, const std::string& email) {
        try {
            if (source_path.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Cannot import town - source path is empty");
                return false;
            }

            if (!std::filesystem::exists(source_path)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Source file does not exist: %s", source_path.c_str());
                return false;
            }

            if (!email.empty()) {
                auto& db = tsto::database::Database::get_instance();

                bool delete_existing_users = utils::configuration::ReadBoolean("Land", "DeleteExistingUsersOnImport", true);

                if (delete_existing_users) {
                    if (db.delete_user(email)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[LAND] Deleted existing user with email: %s before importing town", email.c_str());
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[LAND] No existing user found with email: %s or deletion failed", email.c_str());
                    }
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[LAND] Skipping deletion of existing user with email: %s (DeleteExistingUsersOnImport=false)", email.c_str());
                }
            }

            std::filesystem::create_directories("towns");

            std::string dest_path;
            std::string currency_email;

            if (email.empty()) {
                dest_path = "towns/mytown.pb";
                currency_email = "default";
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[LAND] No email provided, using default filename in towns directory: %s", dest_path.c_str());
            }
            else {
                dest_path = "towns/" + email + ".pb";
                currency_email = email;
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[LAND] Using email-based filename: %s", dest_path.c_str());

                auto& db = tsto::database::Database::get_instance();
                if (!db.update_land_save_path(email, dest_path)) {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_GAME,
                        "[LAND] Failed to update land save path in database for %s", email.c_str());
                }
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Importing town from %s to %s", source_path.c_str(), dest_path.c_str());

            std::filesystem::copy_file(
                source_path,
                dest_path,
                std::filesystem::copy_options::overwrite_existing
            );

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] File copied successfully");

            if (std::filesystem::exists(dest_path)) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[LAND] Target file exists after copy: %s", dest_path.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Target file does not exist after copy: %s", dest_path.c_str());
            }

            if (source_path.find("temp/") == 0) {
                try {
                    std::filesystem::remove(source_path);
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[LAND] Deleted temporary file: %s", source_path.c_str());
                }
                catch (const std::exception& ex) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[LAND] Failed to delete temporary file: %s - %s", source_path.c_str(), ex.what());
                }
            }

            if (!currency_email.empty()) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[LAND] Creating currency file for email: %s", currency_email.c_str());
                create_default_currency_file(currency_email);
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Town imported successfully for %s", email.empty() ? "non-logged-in user" : ("email: " + email).c_str());
            return true;
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Error importing town: %s", ex.what());
            return false;
        }
    }

    void Land::create_default_currency_file(const std::string& email) {
        if (email.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[CURRENCY] Cannot create currency file: email is empty");
            return;
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[CURRENCY] Creating default currency file for email: %s", email.c_str());

        if (!std::filesystem::exists("towns")) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[CURRENCY] Creating towns directory");
            std::filesystem::create_directory("towns");
        }

        int default_donuts = std::stoi(utils::configuration::ReadString("Server", "InitialDonutAmount", "1000"));

        std::string currency_file;
        if (email == "default") {
            currency_file = "towns/currency.txt";   
        }
        else {
            currency_file = "towns/" + email + ".txt";   
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[CURRENCY] Currency file path: %s", currency_file.c_str());

        std::filesystem::path cwd = std::filesystem::current_path();
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[CURRENCY] Current working directory: %s", cwd.string().c_str());

        std::filesystem::path currency_path(currency_file);
        std::filesystem::path parent_path = currency_path.parent_path();
        if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[CURRENCY] Creating parent directory: %s", parent_path.string().c_str());
            std::filesystem::create_directories(parent_path);
        }

        try {
            std::ofstream out(currency_file);
            if (!out.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Failed to open currency file for writing: %s", currency_file.c_str());
                return;
            }

            out << default_donuts;
            out.close();

            if (std::filesystem::exists(currency_file)) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Currency file created successfully: %s with %d donuts",
                    currency_file.c_str(), default_donuts);
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[CURRENCY] Currency file does not exist after creation: %s", currency_file.c_str());
            }
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[CURRENCY] Exception creating currency file: %s", ex.what());
        }
    }

    void Land::handle_town_operations(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[TOWN OPS] Received request: RemoteIP: '%s', URI: '%s'",
                std::string(ctx->remote_ip()).c_str(),
                std::string(ctx->uri()).c_str());

            std::string body = ctx->body().ToString();
            rapidjson::Document doc;
            doc.Parse(body.c_str());

            if (doc.HasParseError()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[TOWN OPS] Invalid JSON in request body");
                headers::set_json_response(ctx);
                ctx->set_response_http_code(400);
                cb("{\"error\":\"Invalid JSON\"}");
                return;
            }

            if (doc.HasMember("operation") && doc["operation"].IsString() &&
                std::string(doc["operation"].GetString()) == "import") {

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[TOWN OPS] Processing import operation");

                std::string email;
                if (doc.HasMember("email") && doc["email"].IsString()) {
                    email = doc["email"].GetString();
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Email provided: %s", email.c_str());
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] No email provided, using default filename");
                }

                if (!doc.HasMember("filePath") || !doc["filePath"].IsString()) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Missing filePath in import request");
                    headers::set_json_response(ctx);
                    ctx->set_response_http_code(400);
                    cb("{\"error\":\"Missing filePath\"}");
                    return;
                }

                std::string temp_file_path = doc["filePath"].GetString();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[TOWN OPS] Temp file path: %s", temp_file_path.c_str());

                if (!std::filesystem::exists("towns")) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Creating towns directory");
                    std::filesystem::create_directory("towns");
                }

                std::string target_file;
                std::string currency_email;

                if (email.empty()) {
                    target_file = "towns/mytown.pb";
                    currency_email = "default";
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] No email provided, using default filename in towns directory: %s", target_file.c_str());
                }
                else {
                    target_file = "towns/" + email + ".pb";
                    currency_email = email;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Using email-based filename: %s", target_file.c_str());
                }

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                    "[TOWN OPS] Target file path: %s", target_file.c_str());

                try {
                    if (!std::filesystem::exists(temp_file_path)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Source file does not exist: %s", temp_file_path.c_str());
                        headers::set_json_response(ctx);
                        ctx->set_response_http_code(500);
                        cb("{\"error\":\"Source file not found\"}");
                        return;
                    }

                    std::filesystem::path cwd = std::filesystem::current_path();
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Current working directory: %s", cwd.string().c_str());

                    std::filesystem::path target_path(target_file);
                    std::filesystem::path parent_path = target_path.parent_path();
                    if (!parent_path.empty() && !std::filesystem::exists(parent_path)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Creating parent directory: %s", parent_path.string().c_str());
                        std::filesystem::create_directories(parent_path);
                    }

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Copying from %s to %s", temp_file_path.c_str(), target_file.c_str());

                    std::filesystem::copy_file(
                        temp_file_path,
                        target_file,
                        std::filesystem::copy_options::overwrite_existing
                    );

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] File copied successfully");

                    if (std::filesystem::exists(target_file)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Target file exists after copy: %s", target_file.c_str());
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Target file does not exist after copy: %s", target_file.c_str());
                    }

                    std::filesystem::remove(temp_file_path);

                    if (!currency_email.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                            "[TOWN OPS] Creating currency file for email: %s", currency_email.c_str());
                        create_default_currency_file(currency_email);
                    }

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Town file imported successfully: %s", target_file.c_str());

                    headers::set_json_response(ctx);
                    cb("{\"success\":true,\"message\":\"Town imported successfully\"}");
                }
                catch (const std::exception& ex) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[TOWN OPS] Failed to import town file: %s", ex.what());
                    headers::set_json_response(ctx);
                    ctx->set_response_http_code(500);
                    cb("{\"error\":\"Failed to import town file: " + std::string(ex.what()) + "\"}");
                }
                return;
            }

        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[TOWN OPS] Exception: %s", ex.what());
            headers::set_json_response(ctx);
            ctx->set_response_http_code(500);
            cb("{\"error\":\"Internal server error\"}");
        }
    }

    void Land::create_blank_town() {
        thread_local static Data::LandMessage land_proto;
        land_proto.Clear();

        auto* friend_data = land_proto.mutable_frienddata();
        friend_data->set_dataversion(72);
        friend_data->set_haslemontree(false);
        friend_data->set_language(0);
        friend_data->set_level(0);
        friend_data->set_name("");
        friend_data->set_rating(0);
        friend_data->set_boardwalktilecount(0);

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[LAND] Created blank town");
    }

    bool Land::static_load_town() {
        Data::LandMessage land_proto;
        std::string town_path = get_default_town_path();

        if (std::filesystem::exists(town_path)) {
            std::ifstream file(town_path, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to open town file: %s", town_path.c_str());
                return false;
            }

            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if (!content.empty() && content[0] == '\x78') {
                std::string decompressed;
                try {
                    decompressed = utils::compression::zlib::decompress(content);
                    content = decompressed;
                }
                catch (const std::exception& ex) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[LAND] Failed to decompress town file: %s - %s", town_path.c_str(), ex.what());
                    return false;
                }
            }

            if (!land_proto.ParseFromString(content)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to parse town file: %s", town_path.c_str());
                return false;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Successfully loaded town from file: %s", town_path.c_str());

            thread_local static Data::LandMessage loaded_proto;
            loaded_proto = land_proto;

            return true;
        }
        else {
            land_proto.Clear();

            auto* friend_data = land_proto.mutable_frienddata();
            friend_data->set_dataversion(72);
            friend_data->set_haslemontree(false);
            friend_data->set_language(0);
            friend_data->set_level(0);
            friend_data->set_name("");
            friend_data->set_rating(0);
            friend_data->set_boardwalktilecount(0);

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Created blank town");

            thread_local static Data::LandMessage created_proto;
            created_proto = land_proto;

            return save_town_to_file("default", land_proto);
        }
    }

    bool Land::save_town() {
        thread_local static Data::LandMessage land_proto;

        std::string filename = "mytown.pb";
        std::string town_path = "towns/" + filename;

        std::filesystem::path path(town_path);
        std::filesystem::path dir = path.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        std::string serialized;
        if (!land_proto.SerializeToString(&serialized)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Failed to serialize town data");
            return false;
        }

        std::string compressed;
        try {
            compressed = utils::compression::zlib::compress(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Failed to compress town data: %s", ex.what());
            return false;
        }

        std::ofstream file(town_path, std::ios::binary);
        if (!file.is_open()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Failed to open town file for writing: %s", town_path.c_str());
            return false;
        }

        file.write(compressed.data(), compressed.size());
        file.close();

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[LAND] Successfully saved town to file: %s", town_path.c_str());
        return true;
    }

    bool Land::instance_load_town() {
        if (std::filesystem::exists(town_path_)) {
            std::ifstream file(town_path_, std::ios::binary);
            if (!file.is_open()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to open town file: %s", town_path_.c_str());
                return false;
            }

            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            file.close();

            if (!content.empty() && content[0] == '\x78') {
                std::string decompressed;
                try {
                    decompressed = utils::compression::zlib::decompress(content);
                    content = decompressed;
                }
                catch (const std::exception& ex) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                        "[LAND] Failed to decompress town file: %s - %s", town_path_.c_str(), ex.what());
                    return false;
                }
            }

            if (!land_proto_.ParseFromString(content)) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                    "[LAND] Failed to parse town file: %s", town_path_.c_str());
                return false;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
                "[LAND] Successfully loaded town from file: %s", town_path_.c_str());
            return true;
        }
        else {
            land_proto_.Clear();

            auto* friend_data = land_proto_.mutable_frienddata();
            friend_data->set_dataversion(72);
            friend_data->set_haslemontree(false);
            friend_data->set_language(0);
            friend_data->set_level(0);
            friend_data->set_name("");
            friend_data->set_rating(0);
            friend_data->set_boardwalktilecount(0);

            return instance_save_town();
        }
    }

    bool Land::instance_save_town() {
        std::filesystem::path path(town_path_);
        std::filesystem::path dir = path.parent_path();
        if (!dir.empty() && !std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }

        std::string serialized;
        if (!land_proto_.SerializeToString(&serialized)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Failed to serialize town data");
            return false;
        }

        std::string compressed;
        try {
            compressed = utils::compression::zlib::compress(serialized);
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Failed to compress town data: %s", ex.what());
            return false;
        }

        std::ofstream file(town_path_, std::ios::binary);
        if (!file.is_open()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "[LAND] Failed to open town file for writing: %s", town_path_.c_str());
            return false;
        }

        file.write(compressed.data(), compressed.size());
        file.close();

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_GAME,
            "[LAND] Successfully saved town to file: %s", town_path_.c_str());
        return true;
    }


}