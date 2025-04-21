#include <std_include.hpp>
#include "auth_new.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "tsto_server.hpp"
#include <serialization.hpp>
#include <AuthData.pb.h> 
#include <WholeLandTokenData.pb.h>
#include <sstream>
#include <random>
#include "tsto/database/database.hpp"
#include "tsto/land/new_land.hpp"
#include <vector>
#include <cctype> 
#include <algorithm> 
#include <filesystem>
#include "configuration.hpp"
#include "cryptography.hpp"
#include "tsto/auth/token_gen.hpp"
#include "tsto/device/device.hpp"

namespace tsto::auth {

static device::DeviceInfo parse_signature_parameter(const std::string& sig_param) {
    device::DeviceInfo info;
    
    try {
        // The sig parameter is a base64-encoded JSON object
        size_t dot_pos = sig_param.find('.');
        if (dot_pos == std::string::npos) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[PARSE SIG] Invalid signature format, no dot separator found");
            return info;
        }
        
        std::string encoded_json = sig_param.substr(0, dot_pos);
        std::string decoded_json = utils::cryptography::base64::decode(encoded_json);
        
        if (decoded_json.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[PARSE SIG] Failed to decode base64 signature");
            return info;
        }
        
        rapidjson::Document doc;
        if (doc.Parse(decoded_json.c_str()).HasParseError()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[PARSE SIG] Failed to parse JSON from signature");
            return info;
        }
        
        if (doc.HasMember("advertisingId") && doc["advertisingId"].IsString()) {
            info.advertising_id = doc["advertisingId"].GetString();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[PARSE SIG] Found advertising ID: %s", info.advertising_id.c_str());
        }
        
        if (doc.HasMember("platformId") && doc["platformId"].IsString()) {
            info.platform_id = doc["platformId"].GetString();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[PARSE SIG] Found platform ID: %s", info.platform_id.c_str());
        }
        
        if (doc.HasMember("platform") && doc["platform"].IsString()) {
            info.platform = doc["platform"].GetString();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[PARSE SIG] Found platform: %s", info.platform.c_str());
            
            // For Android, the platformId is typically the Android ID
            if (info.platform == "android") {
                info.android_id = info.platform_id;
            }
            // For iOS, the platformId is typically the Vendor ID
            else if (info.platform == "ios") {
                info.vendor_id = info.platform_id;
            }
        }
        
        // Extract the "as" identifier (anonymous session ID)
        if (doc.HasMember("as") && doc["as"].IsString()) {
            // Store the "as" identifier in the dedicated as_identifier field
            info.as_identifier = doc["as"].GetString();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[PARSE SIG] Found anonymous session ID (as): %s", info.as_identifier.c_str());
        }
        
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
            "[PARSE SIG] Extracted device info - Platform: %s, Android ID: %s, Vendor ID: %s, Advertising ID: %s, AS ID: %s",
            info.platform.c_str(), info.android_id.c_str(), info.vendor_id.c_str(), 
            info.advertising_id.c_str(), info.as_identifier.c_str());
    }
    catch (const std::exception& ex) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "[PARSE SIG] Exception while parsing signature: %s", ex.what());
    }
    
    return info;
}

std::string url_decode(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.length());
    
    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            int value = 0;
            std::istringstream is(encoded.substr(i + 1, 2));
            if (is >> std::hex >> value) {
                decoded += static_cast<char>(value);
                i += 2;
            } else {
                decoded += encoded[i];
            }
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }
    
    return decoded;
}


// what i worked out is accsess token is disposable and useless only good for auth
std::string Auth::extract_token_from_headers(const evpp::http::ContextPtr& ctx) {
    std::string token;
    
    const char* auth_header = ctx->FindRequestHeader("access_token");
    if (auth_header && auth_header[0] != '\0') {
        token = auth_header;
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[AUTH] Found token in access_token header: %s", token.substr(0, 10).c_str());
        return token;
    }
    
    auth_header = ctx->FindRequestHeader("mh_auth_params");
    if (auth_header && auth_header[0] != '\0') {
        std::string auth_str(auth_header);
        if (auth_str.substr(0, 7) == "Bearer ") {
            token = auth_str.substr(7);
        } else {
            token = auth_str;
        }
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[AUTH] Found token in mh_auth_params header: %s", token.substr(0, 10).c_str());
        return token;
    }
    
    auth_header = ctx->FindRequestHeader("nucleus_token");
    if (auth_header && auth_header[0] != '\0') {
        token = auth_header;
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[AUTH] Found token in nucleus_token header: %s", token.substr(0, 10).c_str());
        return token;
    }
    
    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
        "[AUTH] No token found in headers");
    return "";
}


bool Auth::extract_user_info(const evpp::http::ContextPtr& ctx, 
                       std::string& email, 
                       std::string& user_id, 
                       std::string& mayhem_id, 
                       std::string& access_token) {
    auto& db = tsto::database::Database::get_instance();
    
    //token from headers
    access_token = Auth::extract_token_from_headers(ctx);
    if (access_token.empty()) {
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[AUTH] No access token found in request");
        return false;
    }
    
    //email from token
    if (!db.get_email_by_token(access_token, email) || email.empty()) {
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[AUTH] Could not find email for token: %s", access_token.substr(0, 10).c_str());
        return false;
    }
    
    //user_id and mayhem_id from email
    if (!db.get_user_id(email, user_id) || user_id.empty()) {
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[AUTH] Could not find user_id for email: %s", email.c_str());
        return false;
    }
    
    if (!db.get_mayhem_id(email, mayhem_id) || mayhem_id.empty()) {
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[AUTH] Could not find mayhem_id for email: %s", email.c_str());
        return false;
    }
    
    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
        "[AUTH] Extracted user info - Email: %s, User ID: %s, Mayhem ID: %s",
        email.c_str(), user_id.c_str(), mayhem_id.c_str());
    
    return true;
}

/**
 * Create an anonymous user in the database
 * This is the ONLY place where new users should be created
 * @param ctx HTTP context
 * @param email Output parameter for the generated email
 * @param user_id Output parameter for the generated user ID
 * @param mayhem_id Output parameter for the generated mayhem ID
 * @param access_token Output parameter for the generated access token
 * @param display_name Optional display name for the user
 * @param as_identifier Optional anonymous session identifier from the sig parameter
 * @return True if user was successfully created, false otherwise
 */

bool create_anonymous_user(const evpp::http::ContextPtr& ctx,
                          std::string& email,
                          std::string& user_id,
                          std::string& mayhem_id,
                          std::string& access_token,
                          const std::string& display_name = "Anonymous",
                          const std::string& as_identifier = "") {
    auto& db = tsto::database::Database::get_instance();
    
    bool transaction_started = db.begin_transaction();
    if (!transaction_started) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "[AUTH] Failed to start transaction for user creation");
    } else {
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[AUTH] Started transaction for user creation");
    }
    
    // Generate a new anonymous email with a random string
    email = "anonymous_" + std::to_string(std::rand());
    
    // Get separate IDs for user and mayhem 
    mayhem_id = db.get_next_mayhem_id();
    user_id = db.get_next_user_id();  
    
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
        "[AUTH] Creating anonymous user - Email: %s, User ID: %s, Mayhem ID: %s",
        email.c_str(), user_id.c_str(), mayhem_id.c_str());
    
    // Generate a new access token with a random string
    std::string random_part = std::to_string(std::rand());
    
    // Use the 13-digit user ID directly in the access token
    // This ensures consistency with the user ID format throughout the system
    access_token = "AT0:2.0:3.0:86400:" + random_part + ":" + user_id + ":MPDON";
    
    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
        "[AUTH] Generated access token with 13-digit user ID: %s", access_token.c_str());
    
    // Generate an access code for the anonymous user
    std::string access_code = tsto::token::Token::generate_access_code(user_id);
    
    // Generate a new session key
    std::string session_key = tsto::token::Token::generate_random_string(32);
    
    // Generate a land token (WholeLandToken) - use random string for UUID
    std::string land_token = tsto::token::Token::generate_random_string(36);
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
        "[AUTH] Generated land token for anonymous user: %s", land_token.c_str());
    
    // Get client IP address
    std::string client_ip = std::string(ctx->remote_ip());
    
    // Get device info from cache
    auto& deviceIdCache = tsto::device::DeviceIdCache::get_instance();
    auto device_info = deviceIdCache.get_device_info(client_ip);
    
    // Extract other device identifiers from the cache
    std::string android_id = device_info.android_id;
    std::string vendor_id = device_info.vendor_id;
    std::string advertising_id = device_info.advertising_id;
    std::string platform_id = device_info.platform_id;
    
    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
        "[AUTH] Device info from cache - Android ID: %s, Vendor ID: %s, Advertising ID: %s, Platform ID: %s",
        android_id.c_str(), vendor_id.c_str(), advertising_id.c_str(), platform_id.c_str());
    
    // Get the device ID for this user - first from cache, then from query params
    std::string device_id = deviceIdCache.get_device_id(client_ip);
    
    // Check if we already have a user with this device ID
    std::string existing_email;
    bool device_id_has_user = false;
    
    if (!device_id.empty()) {
        // Check if this device ID is already associated with a user
        device_id_has_user = db.get_user_by_device_id(device_id, existing_email);
        
        if (device_id_has_user && !existing_email.empty()) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[AUTH] Device ID %s is already associated with user %s", 
                device_id.c_str(), existing_email.c_str());
            
            // We're creating a new user, so we need a new device ID
            // This should only happen if handle_connect_auth didn't catch the existing user
            auto now = std::chrono::system_clock::now();
            auto now_str = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
            now_str += "_" + std::to_string(std::rand() % 100000000);
            device_id = utils::cryptography::sha256::compute(now_str, true);
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[AUTH] Generated new device ID: %s for new user (previous ID was already used)", 
                device_id.c_str());
        } else {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[AUTH] Using existing device ID: %s for IP: %s", 
                device_id.c_str(), client_ip.c_str());
        }
    } else {
        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
            "[AUTH] Failed to get device ID from cache for IP: %s, using fallback", client_ip.c_str());
        
        // Fallback: Try to get from query parameter for backward compatibility
        device_id = ctx->GetQuery("device_id");
        
        if (device_id.empty()) {
            // Generate a new device ID if none found
            auto now = std::chrono::system_clock::now();
            auto now_str = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
            now_str += "_" + std::to_string(std::rand() % 100000000);
            device_id = utils::cryptography::sha256::compute(now_str, true);
            
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[AUTH] Generated new device ID: %s for IP: %s", 
                device_id.c_str(), client_ip.c_str());
        }
    }
    
    // Store the device ID in the cache
    device_info.device_id = device_id;
    deviceIdCache.store_device_info(client_ip, device_info);
    
    // Get the anonymous UID from the device cache
    std::string anon_uid = "";
    uint64_t uid = deviceIdCache.get_anon_uid(client_ip);
    if (uid > 0) {
        anon_uid = std::to_string(uid);
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
            "[AUTH] Using anonymous UID from cache: %s for device ID: %s", 
            anon_uid.c_str(), device_id.c_str());
    } else {
        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
            "[AUTH] No anonymous UID found in cache for device ID: %s", device_id.c_str());
    }
    
    // Store the new anonymous user in the database
    if (!db.store_user_id(email, user_id, access_token, mayhem_id, access_code, 
                           "", display_name, "Springfield", "towns/mytown.pb", 
                           device_id, android_id, vendor_id, client_ip, "", advertising_id, platform_id, "", "", session_key, land_token, anon_uid, as_identifier)) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "[AUTH] Failed to store new anonymous user");
            
        // Rollback the transaction if it was started
        if (transaction_started) {
            db.rollback_transaction();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[AUTH] Transaction rolled back due to failure to store user");
        }
        
        return false;
    }
    
    // Store the lnglv_token (which is the same as access_token for anonymous users)
    std::string lnglv_token = access_token;
    if (!db.update_lnglv_token(email, lnglv_token)) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "[AUTH] Failed to store lnglv_token for anonymous user");
        
        if (transaction_started) {
            db.rollback_transaction();
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[AUTH] Transaction rolled back due to failure to store lnglv_token");
        }
        
    }
    
    if (transaction_started) {
        if (!db.commit_transaction()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[AUTH] Failed to commit transaction for user creation");
            db.rollback_transaction();
            return false;
        }
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[AUTH] Transaction committed successfully for user creation");
    }
    
    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
        "[AUTH] Successfully created anonymous user");
    
    return true;
}




void Auth::handle_check_token(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
    const evpp::http::HTTPSendResponseCallback& cb) {
    try {
        helpers::log_request(ctx);

        // Extract mayhem_id from the URL
        std::string uri = std::string(ctx->uri());
        std::vector<std::string> parts;
        size_t pos = 0;
        while ((pos = uri.find('/')) != std::string::npos) {
            parts.push_back(uri.substr(0, pos));
            uri.erase(0, pos + 1);
        }
        parts.push_back(uri);

        // There are two possible URL formats:
        // 1. /mh/games/bg_gameserver_plugin/checkToken/{mayhem_id}/protoWholeLandToken/
        // 2. /mh/games/bg_gameserver_plugin/protoWholeLandToken/{mayhem_id}/
        std::string mayhem_id;
        
        //format 1
        if (parts.size() >= 6 && parts[4] == "checkToken") {
            mayhem_id = parts[5];
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Found mayhem_id in format 1: %s", mayhem_id.c_str());
        }
        //format 2
        else if (parts.size() >= 5 && parts[3] == "protoWholeLandToken") {
            mayhem_id = parts[4];
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Found mayhem_id in format 2: %s", mayhem_id.c_str());
        }

        //token from headers
        std::string token = extract_token_from_headers(ctx);

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[CHECK TOKEN] Mayhem ID: %s, Token: %s", mayhem_id.c_str(), token.c_str());

        //format 2 (/protoWholeLandToken/{mayhem_id}), token might not be required
        bool is_format_2 = parts.size() >= 5 && parts[3] == "protoWholeLandToken";
        
        //check if token is missing (only enforce for format 1)
        if (token.empty() && !is_format_2) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Missing token in request headers");
            ctx->set_response_http_code(400);
            headers::set_xml_response(ctx);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<error code=\"400\" type=\"MISSING_VALUE\" field=\"nucleus_token\"/>");
            return;
        }

        auto& db = tsto::database::Database::get_instance();
        
        //scan database for user data
        std::string email;
        std::string user_access_token;
        std::string whole_land_token;
        
        // First, check if we can find the user by mayhem_id
        if (!db.get_email_by_mayhem_id(mayhem_id, email)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] User not found with mayhem_id: %s", mayhem_id.c_str());
            ctx->set_response_http_code(404);
            headers::set_xml_response(ctx);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<error code=\"404\" type=\"NOT_FOUND\" field=\"mayhemId\"/>");
            return;
        }
        
        // Get access token and land token for this user
        if (!db.get_access_token(email, user_access_token)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Failed to get access token for email: %s", email.c_str());
            ctx->set_response_http_code(500);
            headers::set_xml_response(ctx);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<error code=\"500\" type=\"INTERNAL_SERVER_ERROR\"/>");
            return;
        }
        
        if (token != user_access_token) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Invalid token for mayhem_id: %s", mayhem_id.c_str());
            ctx->set_response_http_code(400);
            headers::set_xml_response(ctx);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<error code=\"400\" type=\"BAD_REQUEST\" field=\"Invalid AcessToken for specified MayhemId\"/>");
            return;
        }
        
        // Get the land token
        db.get_land_token(email, whole_land_token);
        
        // Get session key from database
        std::string session_key;
        bool has_session_key = db.get_session_key(email, session_key);
        
        if (!has_session_key) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Failed to get session key for email: %s", email.c_str());
        } else if (session_key.empty()) {
            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Empty session key found in database for email: %s", email.c_str());
        } else {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Using session key from database: %s for email: %s", 
                session_key.c_str(), email.c_str());
        }
        
        // Create TokenData response 
        Data::TokenData response_message;
		response_message.set_sessionkey(session_key); // Set session key from database ( not random )
        response_message.set_expirationdate(0);
        
        std::string serialized_response;
        if (!response_message.SerializeToString(&serialized_response)) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CHECK TOKEN] Failed to serialize TokenData");
            ctx->set_response_http_code(500);
            headers::set_xml_response(ctx);
            cb("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
               "<error code=\"500\" type=\"INTERNAL_SERVER_ERROR\"/>");
            return;
        }
        
        logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
            "[CHECK TOKEN] Sending response with session key: %s", session_key.c_str());
        
        ctx->set_response_http_code(200);
        headers::set_protobuf_response(ctx);
        cb(serialized_response);
    }
    catch (const std::exception& ex) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "Error in handle_check_token: %s", ex.what());
        ctx->set_response_http_code(500);
        cb("");
    }
}


void Auth::handle_connect_auth(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
    const evpp::http::HTTPSendResponseCallback& cb) {
    try {
        logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH,
            "[CONNECT AUTH] Request from %s", std::string(ctx->remote_ip()).c_str());

        auto& db = tsto::database::Database::get_instance();
        auto client_id = std::string(ctx->GetQuery("client_id"));
        bool is_ios = (client_id == "simpsons4-ios-client");

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[CONNECT AUTH] Client ID: %s, Platform: %s",
            client_id.c_str(), is_ios ? "iOS" : "Android");

        // Get authenticator login type and grant type from request
        auto authenticator_login_type = std::string(ctx->GetQuery("authenticator_login_type"));
        auto grant_type = std::string(ctx->GetQuery("grant_type"));
        
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[CONNECT AUTH] Grant type: %s, Authenticator login type: %s", 
            grant_type.c_str(), authenticator_login_type.c_str());
        
        // Extract and store device identifiers from the sig parameter
        std::string sig_param = std::string(ctx->GetQuery("sig"));
        std::string client_ip = std::string(ctx->remote_ip());
        
        if (!sig_param.empty()) {
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Found sig parameter, processing device identifiers");
            
            // Parse the signature parameter to extract device identifiers
            auto device_info = parse_signature_parameter(sig_param);
            
            // Store the device identifiers in the cache
            auto& deviceIdCache = device::DeviceIdCache::get_instance();
            
            // Update the device info in the cache
            if (!device_info.android_id.empty() || !device_info.vendor_id.empty() || 
                !device_info.advertising_id.empty() || !device_info.platform_id.empty()) {
                
                deviceIdCache.update_device_info(
                    client_ip,
                    device_info.android_id,
                    device_info.vendor_id,
                    device_info.advertising_id,
                    device_info.platform,
                    device_info.platform_id
                );
                
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Stored device identifiers for IP: %s", client_ip.c_str());
            }
        }

        // Handle mobile_anonymous login (matches connect.controller.js implementation)
        if (authenticator_login_type == "mobile_anonymous") {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Mobile anonymous login");

            // Extract the "as" identifier from the signature
            std::string as_identifier = "";
            if (!sig_param.empty()) {
                // Parse the signature parameter to extract device identifiers
                auto device_info = parse_signature_parameter(sig_param);
                as_identifier = device_info.as_identifier; // The "as" value is now stored in as_identifier
                
                if (!as_identifier.empty()) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Found anonymous session ID (as): %s", as_identifier.c_str());
                }
            }
            
            // Get the device ID from the cache
            auto& deviceIdCache = device::DeviceIdCache::get_instance();
            std::string device_id = deviceIdCache.get_device_id(client_ip);
            
            // Variables for user information
            std::string email;
            std::string user_id;
            std::string mayhem_id;
            std::string access_token;
            std::string display_name = "Anonymous";
            bool user_exists = false;
            
            // First check if a user already exists with this "as" identifier
            if (!as_identifier.empty() && db.get_user_by_as_identifier(as_identifier, email)) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Found existing user with email: %s for AS identifier: %s", 
                    email.c_str(), as_identifier.c_str());
                
                // Get the user's information
                if (db.get_user_id(email, user_id) && 
                    db.get_mayhem_id(email, mayhem_id) && 
                    db.get_access_token(email, access_token)) {
                    
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Using existing user by AS identifier - Email: %s, User ID: %s, Mayhem ID: %s",
                        email.c_str(), user_id.c_str(), mayhem_id.c_str());
                    
                    user_exists = true;
                }
            }
            // If no user found by AS identifier, check by device ID  idont no if i shud do this here but oh well fuk it
            else if (!device_id.empty() && !user_exists) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Found device ID: %s for IP: %s", device_id.c_str(), client_ip.c_str());
                
                // Check if a user already exists with this device ID MAYBE YEET THIS 
                if (db.get_user_by_device_id(device_id, email)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Found existing user with email: %s for device ID: %s", 
                        email.c_str(), device_id.c_str());
                    
                    // Get the user's information
                    if (db.get_user_id(email, user_id) && 
                        db.get_mayhem_id(email, mayhem_id) && 
                        db.get_access_token(email, access_token)) {
                        
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[CONNECT AUTH] Using existing user by device ID - Email: %s, User ID: %s, Mayhem ID: %s",
                            email.c_str(), user_id.c_str(), mayhem_id.c_str());
                        
                        user_exists = true;
                        
                        if (!as_identifier.empty()) {
                            if (db.update_as_identifier(email, as_identifier)) {
                                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                    "[CONNECT AUTH] Updated AS identifier for existing user: %s", email.c_str());
                            }
                        }
                    }
                }
            }
            
            //Response for both new and existing users
            if ((user_exists || create_anonymous_user(ctx, email, user_id, mayhem_id, access_token, display_name, as_identifier))) {
                headers::set_json_response(ctx);
                rapidjson::Document doc;
                doc.SetObject();
                auto& allocator = doc.GetAllocator();

                std::string response_type_str = std::string(ctx->GetQuery("response_type"));
                
                std::string decoded_response_type = url_decode(response_type_str);
                
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Response type (raw): %s", response_type_str.c_str());
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Response type (decoded): %s", decoded_response_type.c_str());
                
                std::vector<std::string> response_types;
                std::string token;
                std::istringstream response_stream(decoded_response_type);
                while (std::getline(response_stream, token, ' ')) {
                    if (!token.empty()) {
                        response_types.push_back(token);
                    }
                }

                std::string access_code;
                if (user_exists) {
                    if (!db.get_access_code(email, access_code) || access_code.empty()) {
                        // access_code = tsto::token::Token::generate_access_code(user_id); // Access code generation not needed
                        if (!db.update_access_code(email, access_code)) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[CONNECT AUTH] Failed to update access code for existing user: %s", email.c_str());
                        }
                    }
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Using access code for existing user: %s", access_code.c_str());
                }
                else {
                    // access_code = tsto::token::Token::generate_access_code(user_id); // Access code generation not needed
                    if (!db.update_access_code(email, access_code)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[CONNECT AUTH] Failed to update access code for new user: %s", email.c_str());
                    }
                }


                // Add code to response if explicitly requested - IG THIS FOR LOGGED IN USER 
                bool include_code = false;
                bool include_lnglv_token = false;
                
                for (const auto& type : response_types) {
                    if (type == "code") {
                        include_code = true;
                    } else if (type == "lnglv_token") {
                        include_lnglv_token = true;
                    }
                }
                
                if (include_code) {
                    doc.AddMember("code", rapidjson::Value(access_code.c_str(), allocator), allocator);
                }

                // Add lnglv_token to response if requested
                if (include_lnglv_token) {
                    doc.AddMember("lnglv_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
                }

                if (include_code) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Including code in response: %s", access_code.c_str());
                }
                if (include_lnglv_token) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Including lnglv_token in response: %s", access_token.c_str());
                }
                
                std::string response = utils::serialization::serialize_json(doc);
                logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Sending response for %s user", user_exists ? "existing" : "new");

                cb(response);
                return;
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Failed to create anonymous user");
                ctx->set_response_http_code(500);
                cb("{\"message\":\"Internal error\"}");
                return;
            }
        }
        // Handle mobile_ea_account login
        else if (authenticator_login_type == "mobile_ea_account") {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Mobile EA account login");

            // Get sig from request
            std::string sig = std::string(ctx->GetQuery("sig"));
            if (sig.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Missing field: sig");
                ctx->set_response_http_code(400);
                cb("{\"message\":\"Missing field: sig\"}");
                return;
            }

            // Parse sig to get email and cred
            std::string email;
            std::string cred;
            try {
                // Extract the header part (before first dot)
                size_t first_dot = sig.find('.');
                if (first_dot != std::string::npos) {
                    std::string header = sig.substr(0, first_dot);
                    std::string decoded_header = utils::cryptography::base64::decode(header);
                    
                    // Parse the JSON header
                    rapidjson::Document header_doc;
                    header_doc.Parse(decoded_header.c_str());
                    
                    if (!header_doc.HasParseError() && header_doc.IsObject()) {
                        if (header_doc.HasMember("email") && header_doc["email"].IsString()) {
                            email = header_doc["email"].GetString();
                        }
                        if (header_doc.HasMember("cred") && header_doc["cred"].IsString()) {
                            cred = header_doc["cred"].GetString();
                        }
                    }
                }
                
                // If we couldn't extract from header, try the payload
                if (email.empty() || cred.empty()) {
                    size_t second_dot = sig.find('.', first_dot + 1);
                    
                    if (first_dot != std::string::npos && second_dot != std::string::npos) {
                        std::string payload = sig.substr(first_dot + 1, second_dot - first_dot - 1);
                        std::string decoded_payload = utils::cryptography::base64::decode(payload);
                        
                        // Parse the JSON payload
                        rapidjson::Document sig_doc;
                        sig_doc.Parse(decoded_payload.c_str());
                        
                        if (!sig_doc.HasParseError() && sig_doc.IsObject()) {
                            if (sig_doc.HasMember("email") && sig_doc["email"].IsString() && email.empty()) {
                                email = sig_doc["email"].GetString();
                            }
                            if (sig_doc.HasMember("cred") && sig_doc["cred"].IsString() && cred.empty()) {
                                cred = sig_doc["cred"].GetString();
                            }
                        }
                    }
                }
            }
            catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Failed to parse sig: %s", ex.what());
                ctx->set_response_http_code(400);
                cb("{\"message\":\"Invalid sig\"}");
                return;
            }
            
            // Check if both verification methods are disabled
            bool smtp_enabled = utils::configuration::ReadBoolean("SMTP", "Enabled", false);
            bool api_enabled = utils::configuration::ReadBoolean("TSTO_API", "Enabled", false);
            bool override_enabled = utils::configuration::ReadBoolean("Auth", "OverrideEnabled", false);
            std::string override_code = utils::configuration::ReadString("Auth", "OverrideCode", "99999");
            
            // Check for override code if enabled
            if (override_enabled && cred == override_code) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Override code detected for email: %s", email.c_str());
                // Allow login with override code, but keep the email as provided
                if (email.empty()) {
                    email = "override@tsto.app";
                }
            }
            // If both verification methods are disabled, use default credential
            else if (!smtp_enabled && !api_enabled && email.empty()) {
                // Allow any email with the default credential when verification is disabled
                email = "default@example.com";
                cred = utils::configuration::ReadString("Auth", "DefaultCredential", "12345");
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Both SMTP and TSTO_API verification are disabled, using default values");
            }
            
            if (email.empty() || cred.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Invalid sig: missing email or cred");
                ctx->set_response_http_code(400);
                cb("{\"message\":\"Invalid sig\"}");
                return;
            }
            
            // Check if user exists
            std::string existing_user_id;
            std::string existing_access_token;
            std::string existing_access_code;
            bool user_exists = db.get_user_id(email, existing_user_id) && !existing_user_id.empty() &&
                               db.get_access_token(email, existing_access_token) && !existing_access_token.empty() &&
                               db.get_access_code(email, existing_access_code) && !existing_access_code.empty();
            
            std::string response_type_str = std::string(ctx->GetQuery("response_type"));
            
            std::string decoded_response_type = url_decode(response_type_str);
            
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Response type (raw): %s", response_type_str.c_str());
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Response type (decoded): %s", decoded_response_type.c_str());
            
            std::vector<std::string> response_types;
            std::string token;
            std::istringstream response_stream(decoded_response_type);
            while (std::getline(response_stream, token, ' ')) {
                if (!token.empty()) {
                    response_types.push_back(token);
                }
            }
            
            //which response types were requested
            bool include_code = false;
            bool include_lnglv_token = false;
            
            for (const auto& type : response_types) {
                if (type == "code") {
                    include_code = true;
                } else if (type == "lnglv_token") {
                    include_lnglv_token = true;
                }
            }
            
            headers::set_json_response(ctx);
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();
            
            if (user_exists) {
                std::string stored_cred;
                if (db.get_user_cred(email, stored_cred) && stored_cred != cred) {
                    if (!db.update_user_cred(email, cred)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[CONNECT AUTH] Failed to update credential for user: %s", email.c_str());
                    }
                }
                
                //code to response if requested
                bool include_code = false;
                bool include_lnglv_token = false;
                
                for (const auto& type : response_types) {
                    if (type == "code") {
                        include_code = true;
                    } else if (type == "lnglv_token") {
                        include_lnglv_token = true;
                    }
                }
                
                if (include_code) {
                    doc.AddMember("code", rapidjson::Value(existing_access_code.c_str(), allocator), allocator);
                }
                
                //lnglv_token to response if requested
                if (include_lnglv_token) {
                    doc.AddMember("lnglv_token", rapidjson::Value(existing_access_token.c_str(), allocator), allocator);
                }
                
                if (include_code) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Including code in response (existing user): %s", existing_access_code.c_str());
                }
                if (include_lnglv_token) {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Including lnglv_token in response (existing user): %s", existing_access_token.c_str());
                }
            }
        //    else {
        //        // Create a new user
        //        std::string user_id = db.get_next_user_id();
        //        std::string mayhem_id = db.get_next_mayhem_id();
        //        std::string access_token = tsto::token::Token::generate_random_string(32);
        //        std::string access_code = tsto::token::Token::generate_access_code(user_id);
        //        std::string display_name = email.substr(0, email.find('@'));
        //        
        //        // Store the new user in the database
        //        if (!db.store_user_id(email, user_id, access_token, mayhem_id, access_code, 
        //                             cred, display_name, display_name, "towns/" + email + ".pb", 
        //                             "", "", "", "", "", "", "", "", "", "")) {
        //            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
        //                "[CONNECT AUTH] Failed to store new user: %s", email.c_str());
        //            ctx->set_response_http_code(500);
        //            cb("{\"message\":\"Internal error\"}");
        //            return;
        //        }
        //        
        //        // Use the same response type flags from above
        //        if (include_code) {
        //            doc.AddMember("code", rapidjson::Value(access_code.c_str(), allocator), allocator);
        //        }
        //        
        //        // Add lnglv_token to response if explicitly requested
        //        if (include_lnglv_token) {
        //            doc.AddMember("lnglv_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
        //        }
        //    }
        //    
        //    // Logging is now done inside the if/else blocks
        //    
        //    std::string response = utils::serialization::serialize_json(doc);
        //    logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
        //        "[CONNECT AUTH] Sending response for mobile_ea_account login");
        //    
        //    cb(response);
        //    return;
        //}
        
        //standard OAuth grant types
        else if (grant_type == "authorization_code") {
            // Handle authorization code grant type
            auto code = std::string(ctx->GetQuery("code"));
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Authorization code: %s", code.c_str());

            // Try to find the user by access code
            std::string email;
            std::string user_id;
            std::string mayhem_id;
            std::string stored_access_token;
            std::string cred;
            bool found_user = false;
            
            if (db.get_email_by_access_code(code, email) && !email.empty()) {
                found_user = db.get_user_id(email, user_id) && !user_id.empty() &&
                             db.get_mayhem_id(email, mayhem_id) && !mayhem_id.empty() &&
                             db.get_access_token(email, stored_access_token) && !stored_access_token.empty() &&
                             db.get_user_cred(email, cred);
            }
            
            if (found_user) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Found user by access code - Email: %s, User ID: %s",
                    email.c_str(), user_id.c_str());

                std::string device_id = ctx->GetQuery("device_id");

                if (!device_id.empty() && !db.update_device_id(email, device_id)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Failed to update device ID for user: %s", email.c_str());
                }

                headers::set_json_response(ctx);
                rapidjson::Document doc;
                doc.SetObject();
                auto& allocator = doc.GetAllocator();

                // Generate refresh token
                std::string refresh_token_base = "RT0" + stored_access_token.substr(3);  // Replace AT0 with RT0
                std::string refresh_token = refresh_token_base + ".MpDW6wVO8Ek79nu6jxMdSQwOqP";
                
                // Generate ID token
                std::string id_token_header = utils::cryptography::base64::encode("{\"typ\":\"JWT\",\"alg\":\"HS256\"}");
                
                rapidjson::Document id_token_body_doc;
                id_token_body_doc.SetObject();
                id_token_body_doc.AddMember("aud", rapidjson::Value(client_id.c_str(), allocator), allocator);
                id_token_body_doc.AddMember("iss", "accounts.ea.com", allocator);
                id_token_body_doc.AddMember("iat", (int64_t)(std::time(nullptr)), allocator);
                id_token_body_doc.AddMember("exp", (int64_t)(std::time(nullptr)) + 368435455, allocator); // About 11 years
                id_token_body_doc.AddMember("pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                id_token_body_doc.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                id_token_body_doc.AddMember("persona_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                id_token_body_doc.AddMember("pid_type", "AUTHENTICATOR_ANONYMOUS", allocator);
                id_token_body_doc.AddMember("auth_time", 0, allocator);
                
                std::string id_token_body = utils::cryptography::base64::encode(
                    utils::serialization::serialize_json(id_token_body_doc)
                );
                
                std::string hex_sig = "2Tok8RykmQD41uWDv5mI7JTZ7NIhcZAIPtiBm4Z5";
                std::string id_token = id_token_header + "." + id_token_body + "." +
                    utils::cryptography::base64::encode(hex_sig);
                
                // Get lnglv_token from database
                std::string lnglv_token = stored_access_token; // Default to access_token -- IDK IF THIS CORRECT EITHER
                std::string db_lnglv_token;
                if (db.get_lnglv_token(email, db_lnglv_token) && !db_lnglv_token.empty()) {
                    lnglv_token = db_lnglv_token;
                }
                
                doc.AddMember("access_token", rapidjson::Value(stored_access_token.c_str(), allocator), allocator);
                doc.AddMember("token_type", "Bearer", allocator);
                doc.AddMember("expires_in", 368435455, allocator); // About 11 years
                doc.AddMember("refresh_token", rapidjson::Value(refresh_token.c_str(), allocator), allocator);
                doc.AddMember("refresh_token_expires_in", 368435455, allocator); // About 11 years
                doc.AddMember("id_token", rapidjson::Value(id_token.c_str(), allocator), allocator);
                doc.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);

                std::string response = utils::serialization::serialize_json(doc);
                logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Sending response for authorization_code grant");

                cb(response);
                return;
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Could not find user by access code: %s", code.c_str());
            }
        }
        else if (grant_type == "refresh_token") {
            //refresh token grant type
            auto refresh_token = std::string(ctx->GetQuery("refresh_token"));
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT AUTH] Refresh token: %s", refresh_token.substr(0, 10).c_str());

            std::string access_token;
            if (refresh_token.length() > 3 && refresh_token.substr(0, 3) == "RT0") {
                access_token = "AT0" + refresh_token.substr(3);
                
                size_t last_dot = access_token.find_last_of('.');
                if (last_dot != std::string::npos) {
                    access_token = access_token.substr(0, last_dot);
                }
            }

            if (!access_token.empty()) {
                std::string email;
                std::string user_id;
                std::string mayhem_id;

                if (db.get_email_by_token(access_token, email) && !email.empty() &&
                    db.get_user_id(email, user_id) && !user_id.empty() &&
                    db.get_mayhem_id(email, mayhem_id) && !mayhem_id.empty()) {
                    
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Found user by access token - Email: %s, User ID: %s",
                        email.c_str(), user_id.c_str());

                    headers::set_json_response(ctx);
                    rapidjson::Document doc;
                    doc.SetObject();
                    auto& allocator = doc.GetAllocator();

                    std::string new_refresh_token_base = "RT0" + access_token.substr(3);  // Replace AT0 with RT0
                    std::string new_refresh_token = new_refresh_token_base + ".MpDW6wVO8Ek79nu6jxMdSQwOqP";
                    
                    std::string id_token_header = utils::cryptography::base64::encode("{\"typ\":\"JWT\",\"alg\":\"HS256\"}");
                    
                    rapidjson::Document id_token_body_doc;
                    id_token_body_doc.SetObject();
                    id_token_body_doc.AddMember("aud", rapidjson::Value(client_id.c_str(), allocator), allocator);
                    id_token_body_doc.AddMember("iss", "accounts.ea.com", allocator);
                    id_token_body_doc.AddMember("iat", (int64_t)(std::time(nullptr)), allocator);
                    id_token_body_doc.AddMember("exp", (int64_t)(std::time(nullptr)) + 368435455, allocator); // About 11 years
                    id_token_body_doc.AddMember("pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                    id_token_body_doc.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                    id_token_body_doc.AddMember("persona_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                    id_token_body_doc.AddMember("pid_type", "AUTHENTICATOR_ANONYMOUS", allocator);
                    id_token_body_doc.AddMember("auth_time", 0, allocator);
                    
                    std::string id_token_body = utils::cryptography::base64::encode(
                        utils::serialization::serialize_json(id_token_body_doc)
                    );
                    
                    std::string hex_sig = "2Tok8RykmQD41uWDv5mI7JTZ7NIhcZAIPtiBm4Z5";
                    std::string id_token = id_token_header + "." + id_token_body + "." +
                        utils::cryptography::base64::encode(hex_sig);
                    
                    std::string lnglv_token = access_token; 
                    std::string db_lnglv_token;
                    if (db.get_lnglv_token(email, db_lnglv_token) && !db_lnglv_token.empty()) {
                        lnglv_token = db_lnglv_token;
                    }
                    
                    // Add tokens to response
                    doc.AddMember("access_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
                    doc.AddMember("token_type", "Bearer", allocator);
                    doc.AddMember("expires_in", 368435455, allocator); // About 11 years
                    doc.AddMember("refresh_token", rapidjson::Value(new_refresh_token.c_str(), allocator), allocator);
                    doc.AddMember("refresh_token_expires_in", 368435455, allocator); // About 11 years
                    doc.AddMember("id_token", rapidjson::Value(id_token.c_str(), allocator), allocator);
                    doc.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);

                    std::string response = utils::serialization::serialize_json(doc);
                    logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Sending response for refresh_token grant");

                    cb(response);
                    return;
                }
                else {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[CONNECT AUTH] Could not find user by access token from refresh token");
                }
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Could not extract access token from refresh token");
            }
        }
        //else if (grant_type == "client_credentials") {
        //    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
        //        "[CONNECT AUTH] Anonymous login with client_credentials grant");

        //    std::string email;
        //    std::string user_id;
        //    std::string mayhem_id;
        //    std::string access_token;
        //    std::string display_name = "Guest_" + std::to_string(std::rand()).substr(0, 8);

        //    if (create_anonymous_user(ctx, email, user_id, mayhem_id, access_token, display_name)) {
        //        // Prepare the response
        //        headers::set_json_response(ctx);
        //        rapidjson::Document doc;
        //        doc.SetObject();
        //        auto& allocator = doc.GetAllocator();

        //        // Generate refresh token
        //        std::string refresh_token_base = "RT0" + access_token.substr(3);  // Replace AT0 with RT0
        //        std::string refresh_token = refresh_token_base + ".MpDW6wVO8Ek79nu6jxMdSQwOqP";
        //        
        //        // Generate ID token
        //        std::string id_token_header = utils::cryptography::base64::encode("{\"typ\":\"JWT\",\"alg\":\"HS256\"}");
        //        
        //        rapidjson::Document id_token_body_doc;
        //        id_token_body_doc.SetObject();
        //        id_token_body_doc.AddMember("aud", rapidjson::Value(client_id.c_str(), allocator), allocator);
        //        id_token_body_doc.AddMember("iss", "accounts.ea.com", allocator);
        //        id_token_body_doc.AddMember("iat", (int64_t)(std::time(nullptr)), allocator);
        //        id_token_body_doc.AddMember("exp", (int64_t)(std::time(nullptr)) + 368435455, allocator); // About 11 years
        //        id_token_body_doc.AddMember("pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
        //        id_token_body_doc.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
        //        id_token_body_doc.AddMember("persona_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
        //        id_token_body_doc.AddMember("pid_type", "AUTHENTICATOR_ANONYMOUS", allocator);
        //        id_token_body_doc.AddMember("auth_time", 0, allocator);
        //        
        //        std::string id_token_body = utils::cryptography::base64::encode(
        //            utils::serialization::serialize_json(id_token_body_doc)
        //        );
        //        
        //        std::string hex_sig = "2Tok8RykmQD41uWDv5mI7JTZ7NIhcZAIPtiBm4Z5";
        //        std::string id_token = id_token_header + "." + id_token_body + "." +
        //            utils::cryptography::base64::encode(hex_sig);
        //        
        //        // Get lnglv_token from database (same as access_token for new users)
        //        std::string lnglv_token = access_token;
        //        
        //        // Add tokens to response
        //        doc.AddMember("access_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
        //        doc.AddMember("token_type", "Bearer", allocator);
        //        doc.AddMember("expires_in", 368435455, allocator); // About 11 years
        //        doc.AddMember("refresh_token", rapidjson::Value(refresh_token.c_str(), allocator), allocator);
        //        doc.AddMember("refresh_token_expires_in", 368435455, allocator); // About 11 years
        //        doc.AddMember("id_token", rapidjson::Value(id_token.c_str(), allocator), allocator);
        //        doc.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);

        //        std::string response = utils::serialization::serialize_json(doc);
        //        logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
        //            "[CONNECT AUTH] Sending response for client_credentials grant");

        //        cb(response);
        //        return;
        //    }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT AUTH] Failed to create anonymous user");
            }
        }

        // If we get here, something went wrong
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "[CONNECT AUTH] Failed to handle grant type: %s", grant_type.c_str());
        
        ctx->set_response_http_code(400);
        cb("{\"error\":\"invalid_grant\",\"error_description\":\"The provided authorization grant is invalid\"}");
    }
    catch (const std::exception& ex) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "Error in handle_connect_auth: %s", ex.what());
        ctx->set_response_http_code(500);
        cb("");
    }
}



void Auth::handle_connect_tokeninfo(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
    const evpp::http::HTTPSendResponseCallback& cb) {
    try {
        logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH,
            "[CONNECT TOKENINFO] Request from %s", std::string(ctx->remote_ip()).c_str());

        std::string access_token = ctx->GetQuery("access_token");
        if (access_token.empty()) {
            access_token = extract_token_from_headers(ctx);
        }

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[CONNECT TOKENINFO] Access token: %s", access_token.substr(0, 10).c_str());

        if (access_token.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] No access token provided");
            ctx->set_response_http_code(400);
            cb("{\"error\":\"invalid_request\",\"error_description\":\"No access token provided\"}");
            return;
        }

        auto& db = tsto::database::Database::get_instance();
        std::string email;
        bool valid = db.validate_access_token(access_token, email);

        if (!valid || email.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Invalid access token");
            ctx->set_response_http_code(400);
            cb("{\"error\":\"invalid_token\",\"error_description\":\"The access token is invalid\"}");
            return;
        }

        std::string user_id;
        std::string mayhem_id;
        std::string display_name;

        if (!db.get_user_id(email, user_id) || user_id.empty() ||
            !db.get_mayhem_id(email, mayhem_id) || mayhem_id.empty() ||
            !db.get_display_name(email, display_name)) {
            
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKENINFO] Could not retrieve user information for email: %s", email.c_str());
            ctx->set_response_http_code(500);
            cb("{\"error\":\"server_error\",\"error_description\":\"Could not retrieve user information\"}");
            return;
        }

        headers::set_json_response(ctx);
        rapidjson::Document doc;
        doc.SetObject();
        auto& allocator = doc.GetAllocator();

        bool is_anonymous = email.empty() || email.find("anonymous_") == 0;

        // Use user_id directly as persona_id
        std::string persona_id = user_id;
        
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[CONNECT TOKENINFO] Using user_id directly as persona_id: %s", 
            user_id.c_str());

        doc.AddMember("client_id", rapidjson::Value("long_live_token", allocator), allocator);
        doc.AddMember("expires_in", 368435455, allocator); // About 11 years
        doc.AddMember("persona_id", rapidjson::Value(persona_id.c_str(), allocator), allocator);
        doc.AddMember("pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
        doc.AddMember("pid_type", rapidjson::Value(is_anonymous ? "AUTHENTICATOR_ANONYMOUS" : "NUCLEUS", allocator), allocator);
        doc.AddMember("scope", rapidjson::Value("offline basic.antelope.links.bulk openid signin antelope-rtm-readwrite search.identity basic.antelope basic.identity basic.persona antelope-inbox-readwrite", allocator), allocator);
        doc.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);

        const char* check_underage = ctx->FindRequestHeader("x-check-underage");
        if (check_underage && std::string(check_underage) == "true") {
            doc.AddMember("is_underage", false, allocator);
        }

        const char* include_authenticators = ctx->FindRequestHeader("x-include-authenticators");
        if (include_authenticators && std::string(include_authenticators) == "true") {
            rapidjson::Value authenticators(rapidjson::kArrayType);
            
            // Always add AUTHENTICATOR_ANONYMOUS
            rapidjson::Value anon_auth(rapidjson::kObjectType);
            anon_auth.AddMember("authenticator_pid_id", rapidjson::Value(persona_id.c_str(), allocator), allocator);
            anon_auth.AddMember("authenticator_type", rapidjson::Value("AUTHENTICATOR_ANONYMOUS", allocator), allocator);
            authenticators.PushBack(anon_auth, allocator);
            
            // Add NUCLEUS if not anonymous
            if (!is_anonymous) {
                rapidjson::Value nucleus_auth(rapidjson::kObjectType);
                nucleus_auth.AddMember("authenticator_pid_id", rapidjson::Value(persona_id.c_str(), allocator), allocator);
                nucleus_auth.AddMember("authenticator_type", rapidjson::Value("NUCLEUS", allocator), allocator);
                authenticators.PushBack(nucleus_auth, allocator);
            }
            
            doc.AddMember("authenticators", authenticators, allocator);
        }
        
        const char* include_stopprocess = ctx->FindRequestHeader("x-include-stopprocess");
        if (include_stopprocess && std::string(include_stopprocess) == "true") {
            doc.AddMember("stopProcess", rapidjson::Value("OFF", allocator), allocator);
        }
        
        const char* include_tid = ctx->FindRequestHeader("x-include-tid");
        if (include_tid && std::string(include_tid) == "true") {
            doc.AddMember("telemetry_id", rapidjson::Value(persona_id.c_str(), allocator), allocator);
        }

        std::string response = utils::serialization::serialize_json(doc);
        logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
            "[CONNECT TOKENINFO] Sending response for user: %s", email.c_str());

        cb(response);
    }
    catch (const std::exception& ex) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "Error in handle_connect_tokeninfo: %s", ex.what());
        ctx->set_response_http_code(500);
        cb("");
    }
}


void Auth::handle_connect_token(evpp::EventLoop*, const evpp::http::ContextPtr& ctx,
    const evpp::http::HTTPSendResponseCallback& cb) {
    try {
        logger::write(logger::LOG_LEVEL_INCOMING, logger::LOG_LABEL_AUTH,
            "[CONNECT TOKEN] Request from %s", std::string(ctx->remote_ip()).c_str());

        auto& db = tsto::database::Database::get_instance();
        auto client_id = std::string(ctx->GetQuery("client_id"));
        bool is_ios = (client_id == "simpsons4-ios-client");

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[CONNECT TOKEN] Client ID: %s, Platform: %s",
            client_id.c_str(), is_ios ? "iOS" : "Android");

        auto grant_type = std::string(ctx->GetQuery("grant_type"));
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[CONNECT TOKEN] Grant type: %s", grant_type.c_str());

        if (grant_type == "authorization_code" || grant_type == "add_authenticator") {
            auto code = std::string(ctx->GetQuery("code"));
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Code: %s", code.c_str());
                
            std::string decoded_code = url_decode(code);
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Decoded code: %s", decoded_code.c_str());

            std::string email;
            std::string user_id;
            std::string access_token;
            std::string mayhem_id;
            
            bool found_user = false;
            
            if (db.get_email_by_access_code(decoded_code, email) && !email.empty()) {
                if (db.get_user_id(email, user_id) && !user_id.empty() &&
                    db.get_mayhem_id(email, mayhem_id) && !mayhem_id.empty() &&
                    db.get_access_token(email, access_token) && !access_token.empty()) {
                    found_user = true;
                }
            }
            
            if (!found_user) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] No user found with access code: %s", code.c_str());
                ctx->set_response_http_code(400);
                cb("{\"message\":\"No user could be found with that UserAccessCode\"}");
                return;
            }

            bool is_anonymous = email.empty() || email.find("anonymous_") == 0;

            headers::set_json_response(ctx);
            rapidjson::Document doc;
            doc.SetObject();
            auto& allocator = doc.GetAllocator();

            doc.AddMember("access_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
            
            doc.AddMember("expires_in", 368435455, allocator);
            
            rapidjson::Document jwt_payload;
            jwt_payload.SetObject();
            jwt_payload.AddMember("aud", rapidjson::Value("simpsons4-android-client", allocator), allocator);
            jwt_payload.AddMember("iss", rapidjson::Value("accounts.ea.com", allocator), allocator);
            jwt_payload.AddMember("iat", rapidjson::Value(static_cast<int64_t>(std::time(nullptr))), allocator);
            jwt_payload.AddMember("exp", rapidjson::Value(static_cast<int64_t>(std::time(nullptr) + 368435455)), allocator);
            jwt_payload.AddMember("pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            jwt_payload.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            
            int persona_id = 0;
            try {
                persona_id = std::stoi(user_id);
            } catch (const std::exception& ex) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] Failed to convert user_id to integer: %s", ex.what());
            }
            jwt_payload.AddMember("persona_id", persona_id, allocator);
            
            jwt_payload.AddMember("pid_type", rapidjson::Value(is_anonymous ? "AUTHENTICATOR_ANONYMOUS" : "NUCLEUS", allocator), allocator);
            jwt_payload.AddMember("auth_time", 0, allocator);
            
            // Create JWT token
            std::string id_token_header = utils::cryptography::base64::encode("{\"typ\":\"JWT\",\"alg\":\"HS256\"}");
            std::string id_token_body = utils::cryptography::base64::encode(utils::serialization::serialize_json(jwt_payload));
            std::string id_token = id_token_header + "." + id_token_body + "." + 
                                  utils::cryptography::base64::encode("2Tok8RykmQD41uWDv5mI7JTZ7NIhcZAIPtiBm4Z5");
            
            doc.AddMember("id_token", rapidjson::Value(id_token.c_str(), allocator), allocator);
            doc.AddMember("refresh_token", rapidjson::Value("NotImplemented", allocator), allocator);            
            doc.AddMember("refresh_token_expires_in", 368435455, allocator);
            doc.AddMember("token_type", rapidjson::Value("Bearer", allocator), allocator);
            
            std::string response = utils::serialization::serialize_json(doc);
            logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Sending response for authorization_code grant type");
            
            cb(response);
            return;
        }
        else if (grant_type == "remove_authenticator") {
            // This is the ONLY place where new users should be created
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Grant type: remove_authenticator");

            // Check if we have a valid access token to log out first
            std::string access_token = extract_token_from_headers(ctx);
            
            // Try to find the email for this token and log out the user
            std::string email;
            bool user_deleted = false;
            if (!access_token.empty() && db.get_email_by_token(access_token, email)) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] Found email for access token: %s", email.c_str());

                // Delete the user completely
                if (db.delete_user(email)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[CONNECT TOKEN] Successfully deleted user: %s", email.c_str());
                    user_deleted = true;
                    
                    // Verify user was deleted by trying to find them again
                    std::string verify_email;
                    if (db.get_email_by_token(access_token, verify_email)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[CONNECT TOKEN] User deletion verification failed - user still exists: %s", email.c_str());
                        user_deleted = false;
                    } else {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[CONNECT TOKEN] User deletion verified - user no longer exists: %s", email.c_str());
                    }
                } else {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[CONNECT TOKEN] Failed to delete user: %s", email.c_str());
                }
            }

            // Create a new anonymous user
            std::string anon_email;
            std::string user_id;
            std::string mayhem_id;
            std::string new_access_token;

            if (create_anonymous_user(ctx, anon_email, user_id, mayhem_id, new_access_token, "Anonymous")) {
                // Load or create a new town
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] Creating new town as mytown.pb after logout");



                headers::set_json_response(ctx);
                rapidjson::Document doc;
                doc.SetObject();
                auto& allocator = doc.GetAllocator();

                std::string refresh_token_base = "RT0" + new_access_token.substr(3);  // Replace AT0 with RT0
                std::string refresh_token = refresh_token_base + ".MpDW6wVO8Ek79nu6jxMdSQwOqP";
                
                std::string id_token_header = utils::cryptography::base64::encode("{\"typ\":\"JWT\",\"alg\":\"HS256\"}");
                
                rapidjson::Document id_token_body_doc;
                id_token_body_doc.SetObject();
                id_token_body_doc.AddMember("aud", rapidjson::Value(client_id.c_str(), allocator), allocator);
                id_token_body_doc.AddMember("iss", "accounts.ea.com", allocator);
                id_token_body_doc.AddMember("iat", (int64_t)(std::time(nullptr)), allocator);
                id_token_body_doc.AddMember("exp", (int64_t)(std::time(nullptr)) + 368435455, allocator); // About 11 years
                id_token_body_doc.AddMember("pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                id_token_body_doc.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                id_token_body_doc.AddMember("persona_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                id_token_body_doc.AddMember("pid_type", "AUTHENTICATOR_ANONYMOUS", allocator);
                id_token_body_doc.AddMember("auth_time", 0, allocator);
                
                std::string id_token_body = utils::cryptography::base64::encode(
                    utils::serialization::serialize_json(id_token_body_doc)
                );
                
                std::string hex_sig = "2Tok8RykmQD41uWDv5mI7JTZ7NIhcZAIPtiBm4Z5";
                std::string id_token = id_token_header + "." + id_token_body + "." +
                    utils::cryptography::base64::encode(hex_sig);
                
                std::string lnglv_token = new_access_token;
                
                doc.AddMember("access_token", rapidjson::Value(new_access_token.c_str(), allocator), allocator);
                doc.AddMember("token_type", "Bearer", allocator);
                doc.AddMember("expires_in", 368435455, allocator); // About 11 years
                doc.AddMember("refresh_token", rapidjson::Value(refresh_token.c_str(), allocator), allocator);
                doc.AddMember("refresh_token_expires_in", 368435455, allocator); // About 11 years
                doc.AddMember("id_token", rapidjson::Value(id_token.c_str(), allocator), allocator);
                doc.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);

                std::string response = utils::serialization::serialize_json(doc);
                logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] Sending response for remove_authenticator grant");

                cb(response);
                return;
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[CONNECT TOKEN] Failed to create anonymous user");
            }
        }
        else if (grant_type == "refresh_token") {
            auto refresh_token = std::string(ctx->GetQuery("refresh_token"));
            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[CONNECT TOKEN] Refresh token: %s", refresh_token.substr(0, 10).c_str());

            std::string access_token;
            if (refresh_token.length() > 3 && refresh_token.substr(0, 3) == "RT0") {
                access_token = "AT0" + refresh_token.substr(3);
                
                size_t last_dot = access_token.find_last_of('.');
                if (last_dot != std::string::npos) {
                    access_token = access_token.substr(0, last_dot);
                }
            }

            if (!access_token.empty()) {
                std::string email;
                std::string user_id;
                std::string mayhem_id;

                if (db.get_email_by_token(access_token, email) && !email.empty() &&
                    db.get_user_id(email, user_id) && !user_id.empty() &&
                    db.get_mayhem_id(email, mayhem_id) && !mayhem_id.empty()) {
                    
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[CONNECT TOKEN] Found user by access token - Email: %s, User ID: %s",
                        email.c_str(), user_id.c_str());

                    headers::set_json_response(ctx);
                    rapidjson::Document doc;
                    doc.SetObject();
                    auto& allocator = doc.GetAllocator();

                    std::string new_refresh_token_base = "RT0" + access_token.substr(3);  // Replace AT0 with RT0
                    std::string new_refresh_token = new_refresh_token_base + ".MpDW6wVO8Ek79nu6jxMdSQwOqP";
                    
                    std::string id_token_header = utils::cryptography::base64::encode("{\"typ\":\"JWT\",\"alg\":\"HS256\"}");
                    
                    rapidjson::Document id_token_body_doc;
                    id_token_body_doc.SetObject();
                    id_token_body_doc.AddMember("aud", rapidjson::Value(client_id.c_str(), allocator), allocator);
                    id_token_body_doc.AddMember("iss", "accounts.ea.com", allocator);
                    id_token_body_doc.AddMember("iat", (int64_t)(std::time(nullptr)), allocator);
                    id_token_body_doc.AddMember("exp", (int64_t)(std::time(nullptr)) + 368435455, allocator); // About 11 years
                    id_token_body_doc.AddMember("pid_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                    id_token_body_doc.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                    id_token_body_doc.AddMember("persona_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
                    id_token_body_doc.AddMember("pid_type", "AUTHENTICATOR_ANONYMOUS", allocator);
                    id_token_body_doc.AddMember("auth_time", 0, allocator);
                    
                    std::string id_token_body = utils::cryptography::base64::encode(
                        utils::serialization::serialize_json(id_token_body_doc)
                    );
                    
                    std::string hex_sig = "2Tok8RykmQD41uWDv5mI7JTZ7NIhcZAIPtiBm4Z5";
                    std::string id_token = id_token_header + "." + id_token_body + "." +
                        utils::cryptography::base64::encode(hex_sig);
                    
                    std::string lnglv_token = access_token; // Default to access_token
                    std::string db_lnglv_token;
                    if (db.get_lnglv_token(email, db_lnglv_token) && !db_lnglv_token.empty()) {
                        lnglv_token = db_lnglv_token;
                    }
                    
                    doc.AddMember("access_token", rapidjson::Value(access_token.c_str(), allocator), allocator);
                    doc.AddMember("token_type", "Bearer", allocator);
                    doc.AddMember("expires_in", 368435455, allocator); // About 11 years
                    doc.AddMember("refresh_token", rapidjson::Value(new_refresh_token.c_str(), allocator), allocator);
                    doc.AddMember("refresh_token_expires_in", 368435455, allocator); // About 11 years
                    doc.AddMember("id_token", rapidjson::Value(id_token.c_str(), allocator), allocator);
                    doc.AddMember("lnglv_token", rapidjson::Value(lnglv_token.c_str(), allocator), allocator);

                    std::string response = utils::serialization::serialize_json(doc);
                    logger::write(logger::LOG_LEVEL_RESPONSE, logger::LOG_LABEL_AUTH,
                        "[CONNECT TOKEN] Sending response for refresh_token grant");

                    cb(response);
                    return;
                }
                else {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[CONNECT TOKEN] Could not find user by access token from refresh token");
                }
            }
        }

        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "[CONNECT TOKEN] Failed to handle grant type: %s", grant_type.c_str());
        
        ctx->set_response_http_code(400);
        cb("{\"error\":\"invalid_grant\",\"error_description\":\"The provided authorization grant is invalid\"}");
    }
    catch (const std::exception& ex) {
        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
            "Error in handle_connect_token: %s", ex.what());
        ctx->set_response_http_code(500);
        cb("");
    }
}

} // fuk u ea auth i win
