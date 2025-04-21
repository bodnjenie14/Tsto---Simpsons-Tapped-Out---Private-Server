#include <std_include.hpp>
#include "device.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "ClientTelemetry.pb.h"
#include "cryptography.hpp"
#include <algorithm>

namespace tsto::device {


    //correct flow of the game is get device id first not auth 


    std::string Device::generateUniqueDeviceId(const std::string& androidId, const std::string& hwId, const std::string& clientIp) {
        std::string baseString = androidId + hwId + clientIp + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(10000000, 99999999);
        baseString += std::to_string(dis(gen));

        std::string hash = utils::cryptography::sha256::compute(baseString);

        std::string deviceId = "TSTO" + hash.substr(0, 28);

        return deviceId;
    }

    void Device::handle_device_registration(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Request from %s", ctx->remote_ip().data());

            rapidjson::Document request_body;
            std::string body_content = ctx->body().ToString();
            if (request_body.Parse(body_content.c_str()).HasParseError()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Failed to parse JSON body");
                ctx->set_response_http_code(400);
                cb(R"({"status": "error", "message": "Invalid JSON in request body"})");
                return;
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Extracted fields:");
            for (auto it = request_body.MemberBegin(); it != request_body.MemberEnd(); ++it) {
                if (it->name.IsString() && it->value.IsString()) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "%s: %s", it->name.GetString(), it->value.GetString());
                }
                else if (it->name.IsString()) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP, "%s: [non-string value]", it->name.GetString());
                }
            }

            auto& db = tsto::database::Database::get_instance();

            std::string device_id;
            std::string manufacturer;
            std::string model;

            if (request_body.HasMember("deviceIdentifier") && request_body["deviceIdentifier"].IsString()) {
                device_id = request_body["deviceIdentifier"].GetString();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "[DEVICE REGISTRATION] Found device identifier: %s", device_id.c_str());
            }

            if (request_body.HasMember("manufacturer") && request_body["manufacturer"].IsString()) {
                manufacturer = request_body["manufacturer"].GetString();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "[DEVICE REGISTRATION] Found manufacturer: %s", manufacturer.c_str());
            } else if (request_body.HasMember("deviceType") && request_body["deviceType"].IsString()) {
                manufacturer = request_body["deviceType"].GetString();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "[DEVICE REGISTRATION] Found manufacturer from deviceType: %s", manufacturer.c_str());
            }

            if (request_body.HasMember("model") && request_body["model"].IsString()) {
                model = request_body["model"].GetString();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                    "[DEVICE REGISTRATION] Found model: %s", model.c_str());
            }

            std::string email;
            bool user_found = false;
            const char* auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header && strncmp(auth_header, "Basic ", 6) == 0) {
                std::string auth_token = auth_header + 6;     

                std::string decoded_auth = utils::cryptography::base64::decode(auth_token);
                if (!decoded_auth.empty()) {
                    size_t colon_pos = decoded_auth.find(':');
                    if (colon_pos != std::string::npos) {
                        std::string user_id = decoded_auth.substr(0, colon_pos);
                        std::string token = decoded_auth.substr(colon_pos + 1);

                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                            "[DEVICE REGISTRATION] Extracted user_id: %s from Authorization header",
                            user_id.c_str());

                        if (db.get_email_by_user_id(user_id, email)) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                                "[DEVICE REGISTRATION] Found email: %s for user_id: %s",
                                email.c_str(), user_id.c_str());
                            user_found = true;
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                                "[DEVICE REGISTRATION] Could not find email for user_id: %s",
                                user_id.c_str());
                        }
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                            "[DEVICE REGISTRATION] Invalid format in Authorization header");
                    }
                }
                else {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                        "[DEVICE REGISTRATION] Failed to decode Authorization header");
                }
            }

            if (!user_found) {
                const char* token_header = ctx->FindRequestHeader("mh_auth_params");
                if (token_header) {
                    std::string access_token = token_header;
                    if (db.get_email_by_token(access_token, email)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                            "[DEVICE REGISTRATION] Found user by access token: %s -> %s",
                            access_token.c_str(), email.c_str());
                        user_found = true;
                    }
                }
            }
            
            if (!user_found) {
                const char* mh_uid_header = ctx->FindRequestHeader("mh_uid");
                if (mh_uid_header) {
                    std::string mayhem_id = mh_uid_header;
                    if (db.get_email_by_mayhem_id(mayhem_id, email)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                            "[DEVICE REGISTRATION] Found user by mayhem_id: %s -> %s",
                            mayhem_id.c_str(), email.c_str());
                        user_found = true;
                    }
                }
            }


            
            if (!device_id.empty()) {
                if (db.get_user_by_device_id(device_id, email)) {
                    user_found = true;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                        "[DEVICE REGISTRATION] Found user by device identifier: %s -> %s",
                        device_id.c_str(), email.c_str());
                        
                    if (!manufacturer.empty()) {
                        if (db.update_manufacturer(email, manufacturer)) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                                "[DEVICE REGISTRATION] Updated manufacturer for user %s: %s",
                                email.c_str(), manufacturer.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                                "[DEVICE REGISTRATION] Failed to update manufacturer for user %s",
                                email.c_str());
                        }
                    }

                    if (!model.empty()) {
                        if (db.update_model(email, model)) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_SERVER_HTTP,
                                "[DEVICE REGISTRATION] Updated model for user %s: %s",
                                email.c_str(), model.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP,
                                "[DEVICE REGISTRATION] Failed to update model for user %s",
                                email.c_str());
                        }
                    }
                }
                else {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                        "[DEVICE REGISTRATION] No user found for device identifier: %s", device_id.c_str());
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_SERVER_HTTP,
                    "[DEVICE REGISTRATION] No device identifier provided in request");
            }

            rapidjson::Document response;
            response.SetObject();
            auto& allocator = response.GetAllocator();

            response.AddMember("deviceId", rapidjson::Value(device_id.c_str(), allocator), allocator);
            response.AddMember("resultCode", 0, allocator);
            response.AddMember("serverApiVersion", "1.0.0", allocator);

            headers::set_json_response(ctx);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Response sent to %s", ctx->remote_ip().data());

            cb(utils::serialization::serialize_json(response));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_SERVER_HTTP, "[DEVICE REGISTRATION] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb(R"({"status": "error", "message": "Internal Server Error"})");
        }
    }

    void Device::handle_get_anon_uid(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string client_ip = std::string(ctx->remote_ip());
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH, 
                "[ANON UID] Request from %s", client_ip.c_str());

            auto& db = tsto::database::Database::get_instance();
            auto& deviceIdCache = DeviceIdCache::get_instance();
            
            uint64_t uid = deviceIdCache.get_anon_uid(client_ip);
            bool need_new_uid = (uid == 0);
            
            if (uid > 0) {
                std::string existing_email;
                std::string uid_str = std::to_string(uid);
                if (db.get_user_by_anon_uid(uid_str, existing_email) && !existing_email.empty()) {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
                        "[ANON UID] Cached UID %llu is already associated with user %s, need new one",
                        uid, existing_email.c_str());
                    need_new_uid = true;
                }
            }
            
            if (need_new_uid) {
                bool is_unique = false;
                int attempts = 0;
                const int max_attempts = 20;      
                
                while (!is_unique && attempts < max_attempts) {
                    uid = utils::cryptography::random::get_integer() % 90000000000 + 10000000000;
                    std::string uid_str = std::to_string(uid);
                    
                    std::string existing_email;
                    if (db.get_user_by_anon_uid(uid_str, existing_email)) {
                        if (!existing_email.empty()) {
                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                                "[ANON UID] Generated UID %llu already exists for user %s, trying again",
                                uid, existing_email.c_str());
                            attempts++;
                        } else {
                            is_unique = true;
                        }
                    } else {
                        is_unique = true;
                    }
                }
                
                if (attempts >= max_attempts) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[ANON UID] Failed to generate unique UID after %d attempts, using last generated: %llu",
                        max_attempts, uid);
                }
                
                deviceIdCache.store_anon_uid(client_ip, uid);
                
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[ANON UID] Generated and stored new 11-digit UID: %llu", uid);
            } else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[ANON UID] Using existing UID: %llu", uid);
            }
            
            headers::set_json_response(ctx);
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();
            
            doc.AddMember("resultCode", 0, allocator);
            doc.AddMember("serverApiVersion", "1.0.0", allocator);
            doc.AddMember("uid", uid, allocator);
            
            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH, 
                "[ANON UID] Sending uid: %llu", uid);
            
            cb(utils::serialization::serialize_json(doc));
        } catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[ANON UID] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Device::handle_get_device_id(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {

            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH, "[GET DEVICE ID] Request from %s", ctx->remote_ip().data());

            std::string androidId = ctx->GetQuery("androidId");
            std::string hwId = ctx->GetQuery("hwId");
            std::string clientIp = std::string(ctx->remote_ip());

#ifdef DEBUG
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "Request params:\n"
                "localization: %s\n"
                "appVer: %s\n"
                "deviceLanguage: %s\n"
                "appLang: %s\n"
                "apiVer: %s\n"
                "hwId: %s\n"
                "deviceLocale: %s\n"
                "androidId: %s",
                ctx->GetQuery("localization").c_str(),
                ctx->GetQuery("appVer").c_str(),
                ctx->GetQuery("deviceLanguage").c_str(),
                ctx->GetQuery("appLang").c_str(),
                ctx->GetQuery("apiVer").c_str(),
                hwId.c_str(),
                ctx->GetQuery("deviceLocale").c_str(),
                androidId.c_str());
#endif 

            headers::set_json_response(ctx);
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            auto& deviceIdCache = DeviceIdCache::get_instance();

            std::string newDeviceId = deviceIdCache.get_device_id(clientIp);

            if (newDeviceId.empty()) {
                auto now = std::chrono::system_clock::now();
                auto now_str = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count());

                now_str += "_" + std::to_string(std::rand() % 100000000);

                newDeviceId = utils::cryptography::sha256::compute(now_str, true);

                deviceIdCache.store_device_id(clientIp, newDeviceId);

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[GET DEVICE ID] Generated new device ID: %s for IP: %s",
                    newDeviceId.c_str(), clientIp.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[GET DEVICE ID] Using existing device ID: %s for IP: %s",
                    newDeviceId.c_str(), clientIp.c_str());
            }

            doc.AddMember("deviceId", rapidjson::Value(newDeviceId.c_str(), allocator), allocator);
            doc.AddMember("resultCode", 0, allocator);
            doc.AddMember("serverApiVersion", "1.0.0", allocator);

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                "[GET DEVICE ID] Sending deviceId: %s", newDeviceId.c_str());

            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_GAME,
                "Error in handle_get_device_id: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }

    void Device::handle_validate_device_id(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH, "[VALIDATE DEVICE ID] Request from %s", ctx->remote_ip().data());

            std::string deviceId = ctx->GetQuery("eadeviceid");
            std::string androidId = ctx->GetQuery("androidId");
            std::string hwId = ctx->GetQuery("hwId");

#ifdef DEBUG
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "Request params:\n"
                "appVer: %s\n"
                "deviceLanguage: %s\n"
                "appLang: %s\n"
                "apiVer: %s\n"
                "serverEnvironment: %s\n"
                "hwId: %s\n"
                "deviceLocale: %s\n"
                "eadeviceid: %s\n"
                "androidId: %s",
                ctx->GetQuery("appVer").c_str(),
                ctx->GetQuery("deviceLanguage").c_str(),
                ctx->GetQuery("appLang").c_str(),
                ctx->GetQuery("apiVer").c_str(),
                ctx->GetQuery("serverEnvironment").c_str(),
                hwId.c_str(),
                ctx->GetQuery("deviceLocale").c_str(),
                deviceId.c_str(),
                androidId.c_str());
#endif 

            headers::set_json_response(ctx);
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();
            auto& session = tsto::Session::get();


            auto& db = tsto::database::Database::get_instance();
            std::string email;
            std::string access_code;
            std::string lnglv_token;
            bool device_found = false;

            if (!deviceId.empty() && db.get_user_by_device_id(deviceId, email)) {
                device_found = true;
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[VALIDATE DEVICE ID] Found existing device with ID: %s, email: %s", deviceId.c_str(), email.c_str());
            }
            else if (!androidId.empty() && db.check_android_id_exists(androidId, email)) {
                device_found = true;
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[VALIDATE DEVICE ID] Found existing device with Android ID: %s, email: %s", androidId.c_str(), email.c_str());

                std::string stored_device_id;
                if (db.get_device_id(email, stored_device_id) && stored_device_id.empty() && !deviceId.empty()) {
                    if (db.update_device_id(email, deviceId)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[VALIDATE DEVICE ID] Updated device ID for email: %s with ID: %s", email.c_str(), deviceId.c_str());
                    }
                }
            }

            if (device_found && !email.empty()) {
                if (db.get_access_code(email, access_code)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[VALIDATE DEVICE ID] Retrieved access code for email: %s", email.c_str());
                }

                if (db.get_lnglv_token(email, lnglv_token)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[VALIDATE DEVICE ID] Retrieved lnglv token for email: %s", email.c_str());
                }
            }

            doc.AddMember("deviceId", rapidjson::Value(session.device_id.c_str(), allocator), allocator);
            doc.AddMember("resultCode", 0, allocator);
            doc.AddMember("serverApiVersion", "1.0.0", allocator);

            if (device_found) {
                if (!access_code.empty()) {
                    doc.AddMember("accessCode", rapidjson::Value(access_code.c_str(), allocator), allocator);
                }
                if (!lnglv_token.empty()) {
                    doc.AddMember("lnglvToken", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);
                }
                if (!email.empty()) {
                    doc.AddMember("email", rapidjson::Value(email.c_str(), allocator), allocator);
                }
            }

            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                "[VALIDATE DEVICE ID] Sending deviceId: %s, device_found: %s",
                session.device_id.c_str(), device_found ? "true" : "false");

            cb(utils::serialization::serialize_json(doc));
        }
        catch (const std::exception& ex) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH, "[VALIDATE DEVICE ID] Error: %s", ex.what());
            ctx->set_response_http_code(500);
            cb("");
        }
    }
}