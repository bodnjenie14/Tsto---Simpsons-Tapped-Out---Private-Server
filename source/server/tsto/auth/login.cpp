#include <std_include.hpp>
#include "login.hpp"
#include "auth_new.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "tsto_server.hpp"
#include <serialization.hpp>
#include <AuthData.pb.h> 
#include <sstream>
#include <random>
#include "tsto/database/database.hpp"
#include "tsto/land/new_land.hpp"
#include <vector>
#include <cctype> 
#include <algorithm> 
#include <filesystem>
#include <curl/curl.h>
#include "configuration.hpp"
#include <string>
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <regex>
#include <ctime>
#include "tsto/auth/token_gen.hpp"
#include "tsto/device/device.hpp"


namespace tsto::login {

    void Login::handle_progreg_code(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        bool transaction_started = false;
        try {
            std::string body = ctx->body().ToString();
            if (body.empty()) {
                throw std::runtime_error("Empty request body");
            }

            rapidjson::Document doc;
            doc.Parse(body.c_str());
            if (doc.HasParseError() || !doc.IsObject()) {
                throw std::runtime_error("Invalid JSON in request body");
            }

            if (!doc.HasMember("email") || !doc["email"].IsString()) {
                throw std::runtime_error("Missing or invalid 'email' field in request");
            }
            std::string email = doc["email"].GetString();
            std::string filename = email;

            std::string town_name = "Springfield"; // this will be the new system eventually to replace emails

            std::string display_name;
            size_t at_pos = filename.find('@');
            if (at_pos != std::string::npos) {
                display_name = filename.substr(0, at_pos);
                if (!display_name.empty()) {
                    display_name[0] = std::toupper(display_name[0]);
                }
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[PROGREG CODE] Using display name: %s for user: %s",
                display_name.c_str(), filename.c_str());

            std::string user_cred = "";
            if (doc.HasMember("cred") && doc["cred"].IsString()) {
                user_cred = doc["cred"].GetString();
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Credential received for user: %s", email.c_str());

                if (!user_cred.empty()) {
                    tsto::database::Database& db = tsto::database::Database::get_instance();
                    bool update_success = false;
                    for (int attempt = 0; attempt < 3 && !update_success; attempt++) {
                        if (db.update_user_cred(email, user_cred)) {
                            update_success = true;
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Successfully updated credential for user: %s (attempt %d)",
                                email.c_str(), attempt + 1);

                            std::string stored_cred;
                            if (db.get_user_cred(email, stored_cred) && stored_cred == user_cred) {
                                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                    "[PROGREG CODE] Verified credential was correctly stored for user: %s", email.c_str());
                            }
                            else {
                                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                    "[PROGREG CODE] Credential verification failed for user: %s (Stored: %s, Expected: %s)",
                                    email.c_str(), stored_cred.empty() ? "empty" : stored_cred.c_str(), user_cred.c_str());
                                update_success = false;
                            }
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Failed to update credential for user: %s (attempt %d)",
                                email.c_str(), attempt + 1);
                        }
                    }

                    if (!update_success) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] All attempts to update credential failed for user: %s", email.c_str());
                    }
                }
            }

            std::string provided_code = "";
            if (doc.HasMember("code") && doc["code"].IsString()) {
                provided_code = doc["code"].GetString();
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Code provided in request: %s", provided_code.c_str());
            }
            else if (provided_code.empty() && doc.HasMember("cred") && doc["cred"].IsString()) {
                provided_code = doc["cred"].GetString();
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Using credential as verification code: %s", provided_code.c_str());
            }

            std::string access_token;
            const char* auth_header = ctx->FindRequestHeader("Authorization");
            if (auth_header && strncmp(auth_header, "Bearer ", 7) == 0) {
                access_token = auth_header + 7; // Skip "Bearer " prefix
            }

            if (access_token.empty()) {
                throw std::runtime_error("Missing or invalid Authorization header");
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[PROGREG CODE] Access token received: %s", access_token.c_str());


            auto& db = tsto::database::Database::get_instance();

            std::string existing_display_name;
            if (db.get_display_name(email, existing_display_name) && !existing_display_name.empty()) {
                display_name = existing_display_name;
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Using existing display name from database: %s for user: %s",
                    display_name.c_str(), email.c_str());
            }

            std::string existing_email;
            bool token_exists = db.get_email_by_token(access_token, existing_email);

            std::string user_id;
            std::string mayhem_id = "";
            std::string access_code;
            std::string device_id = ""; 
            std::string android_id = "";
            std::string vendor_id = "";
            std::string advertising_id = ""; 
            std::string platform_id = ""; 
            std::string client_ip = ""; 
            std::string combined_id = ""; 
            std::string manufacturer = ""; 
            std::string model = ""; 
            std::string existing_lnglv_token = "";
            std::string existing_session_key = "";
            std::string existing_land_token = "";
            std::string existing_uid = "";
            if (token_exists) {

                std::string stored_user_id;
                if (db.get_user_id(existing_email, stored_user_id)) {
                    user_id = stored_user_id;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Converting anonymous user to registered user. Email: %s -> %s, User ID: %s",
                        existing_email.c_str(), email.c_str(), user_id.c_str());

                    if (db.get_device_id(existing_email, device_id) && !device_id.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving device ID from anonymous user: %s", device_id.c_str());
                    }


                    if (db.get_android_id(existing_email, android_id) && !android_id.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving Android ID from anonymous user: %s", android_id.c_str());
                    }

                    if (db.get_vendor_id(existing_email, vendor_id) && !vendor_id.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving Vendor ID from anonymous user: %s", vendor_id.c_str());
                    }

                    if (db.get_advertising_id(existing_email, advertising_id) && !advertising_id.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving Advertising ID from anonymous user: %s", advertising_id.c_str());
                    }

                    if (db.get_platform_id(existing_email, platform_id) && !platform_id.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving Platform ID from anonymous user: %s", platform_id.c_str());
                    }

                    if (db.get_client_ip(existing_email, client_ip) && !client_ip.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving Client IP from anonymous user: %s", client_ip.c_str());
                    }

                    if (db.get_combined_id(existing_email, combined_id) && !combined_id.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving Combined ID from anonymous user: %s", combined_id.c_str());
                    }

                    if (db.get_manufacturer(existing_email, manufacturer) && !manufacturer.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving Manufacturer from anonymous user: %s", manufacturer.c_str());
                    }

                    if (db.get_model(existing_email, model) && !model.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving Model from anonymous user: %s", model.c_str());
                    }

                    std::string existing_access_code;
                    if (db.get_access_code(existing_email, existing_access_code) && !existing_access_code.empty()) {
                        access_code = existing_access_code;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving access code from anonymous user: %s", access_code.c_str());
                    }


                    if (db.get_lnglv_token(existing_email, existing_lnglv_token) && !existing_lnglv_token.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving lnglv token from anonymous user: %s", existing_lnglv_token.c_str());
                    }

                    if (db.get_session_key(existing_email, existing_session_key) && !existing_session_key.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving session key from anonymous user: %s", existing_session_key.c_str());
                    }

                    if (db.get_land_token(existing_email, existing_land_token) && !existing_land_token.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving land token from anonymous user: %s", existing_land_token.c_str());
                    }

                    std::string anon_uid;
                    if (db.get_anon_uid(existing_email, anon_uid) && !anon_uid.empty()) {
                        existing_uid = anon_uid;
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving anonymous UID from anonymous user: %s", existing_uid.c_str());
                    }
                    else {
                        std::string client_ip = std::string(ctx->remote_ip());
                        auto& deviceIdCache = tsto::device::DeviceIdCache::get_instance();
                        uint64_t uid = deviceIdCache.get_anon_uid(client_ip);
                        if (uid > 0) {
                            existing_uid = std::to_string(uid);
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Retrieved anonymous UID from device cache: %s", existing_uid.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] No anonymous UID found for user: %s", existing_email.c_str());
                        }
                    }
                    
                    std::string as_identifier;
                    if (db.get_as_identifier(existing_email, as_identifier) && !as_identifier.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving AS identifier from anonymous user: %s", as_identifier.c_str());
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] No AS identifier found for user: %s", existing_email.c_str());
                        as_identifier = ""; // Ensure it's empty if not found
                    }


                    std::string existing_device_id;
                    if (db.get_device_id(existing_email, existing_device_id) && !existing_device_id.empty()) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Preserving device ID from anonymous user: %s", existing_device_id.c_str());

                        if (!db.update_device_id(email, existing_device_id)) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Failed to update device ID for email: %s", email.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Successfully updated device ID for email: %s", email.c_str());
                        }
                    }

                    //if (user_cred.empty()) {
                    //    std::string existing_cred;
                    //    if (db.get_user_cred(existing_email, existing_cred) && !existing_cred.empty()) {
                    //        user_cred = existing_cred;
                    //        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    //            "[PROGREG CODE] Using existing credential for user: %s", existing_email.c_str());
                    //    }
                    //}

                    //db.get_mayhem_id(existing_email, mayhem_id);
                    //if (mayhem_id.empty()) {
                    //    mayhem_id = db.get_next_mayhem_id();
                    //    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    //        "[PROGREG CODE] No existing mayhem ID found, generated new mayhem_id: %s", mayhem_id.c_str());
                    //}
                    //else {
                    //    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    //        "[PROGREG CODE] Preserving existing mayhem ID: %s from temporary user", mayhem_id.c_str());
                    //}
                }
            }
            else {
                std::string stored_user_id;
                if (db.get_user_id(filename, stored_user_id)) {
                    user_id = stored_user_id;
                    db.get_mayhem_id(filename, mayhem_id);
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Using existing user_id for %s: %s",
                        filename.c_str(), stored_user_id.c_str());
                }


            }
			//not implemented fully yet this will be the new town name rather than bad practice emails
            if (token_exists && existing_email != email) {

                std::vector<std::string> town_prefixes = {
                    "New", "Old", "East", "West", "North", "South", "Upper", "Lower",
                    "Little", "Big", "Grand", "Royal", "Pleasant", "Happy", "Golden"
                };

                std::vector<std::string> town_suffixes = {
                    "ville", "town", "burg", "field", "port", "haven", "dale", "wood",
                    "ridge", "valley", "creek", "springs", "harbor", "land", "shire"
                };

                std::random_device rd;
                std::mt19937 gen(rd());
                std::uniform_int_distribution<> prefix_dist(0, town_prefixes.size() - 1);
                std::uniform_int_distribution<> suffix_dist(0, town_suffixes.size() - 1);

                if (!display_name.empty()) {
                    std::string capitalized_name = display_name;
                    if (!capitalized_name.empty()) {
                        capitalized_name[0] = std::toupper(capitalized_name[0]);
                    }

                    town_name = town_prefixes[prefix_dist(gen)] + " " + capitalized_name + town_suffixes[suffix_dist(gen)];
                }
                else {
                    town_name = town_prefixes[prefix_dist(gen)] + town_suffixes[suffix_dist(gen)];
                }

                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Generated town name for %s: %s",
                    email.c_str(), town_name.c_str());
            }

            std::string as_identifier = "";
            if (token_exists && existing_email != email) {
                if (db.get_as_identifier(existing_email, as_identifier)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Preserving AS identifier from anonymous user: %s",
                        as_identifier.empty() ? "(empty)" : as_identifier.c_str());
                }
                
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Updating user from %s to %s with the same access token",
                    existing_email.c_str(), email.c_str());

                // Check if the target email already exists and delete it first
                // Do this BEFORE starting the transaction to avoid deadlocks
                std::string existing_target_id;
                if (db.get_user_id(email, existing_target_id)) {
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Target email %s already exists with user ID: %s. Deleting before migration.",
                        email.c_str(), existing_target_id.c_str());

                    if (db.delete_user(email)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Successfully deleted existing user with email: %s before migration",
                            email.c_str());
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to delete existing user with email: %s before migration",
                            email.c_str());
                        // Continue with migration anyway
                    }
                }


                transaction_started = db.begin_transaction();
                if (!transaction_started) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Failed to start database transaction for user migration");
                }
                else {
                    logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Started database transaction for user migration");
                }

                std::string existing_user_id;
                if (db.get_user_id(existing_email, existing_user_id)) {
                    if (db.delete_anonymous_users_by_user_id(existing_user_id)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Successfully deleted anonymous users with user ID: %s",
                            existing_user_id.c_str());
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to delete anonymous users with user ID: %s",
                            existing_user_id.c_str());
                    }
                }

                if (db.delete_anonymous_users_by_token(access_token)) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Successfully deleted anonymous users with token: %s",
                        access_token.substr(0, 10).c_str());
                }
                else {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Failed to delete anonymous users with token: %s",
                        access_token.substr(0, 10).c_str());
                }

                if (existing_email.substr(0, 10) != "anonymous_") {
                    std::string empty_token = "";
                    if (!db.update_access_token(existing_email, empty_token)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to clear access token for old email: %s", existing_email.c_str());
                    }

                    if (!db.update_access_token(email, access_token)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to update access token for new email: %s", email.c_str());
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Successfully associated access token with new email: %s", email.c_str());
                    }
                }
            }

            //verification methods
            bool smtp_enabled = utils::configuration::ReadBoolean("SMTP", "Enabled", false);
            bool api_enabled = utils::configuration::ReadBoolean("TSTO_API", "Enabled", false);

            //1. API if enabled, 2. SMTP if enabled, 3. Use generated code if both disabled
            if (api_enabled) {
                //use API to generate the code - this calls the external TSTO API
                //which will generate a code and return it
                std::string api_code = handle_tsto_api(email, display_name);
                if (!api_code.empty()) {
                    // Store API code in user_cred
                    user_cred = api_code;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Received code from TSTO API for %s: %s",
                        email.c_str(), access_code.c_str());
                }
                else {
                    // API failed, fall back to default code
                    access_code = tsto::token::Token::generate_access_code(user_id);
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] API code generation failed, using fallback code for %s: %s",
                        email.c_str(), access_code.c_str());
                }
            }
            else if (smtp_enabled) {
                // Use SMTP to generate and send the code via email
                // handle_smtp generates a 6-digit code and sends it via email
                std::string smtp_code = handle_smtp(email, display_name);
                if (!smtp_code.empty()) {
                    access_code = smtp_code;
                    // Don't store access code in user_cred
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Using code from SMTP for %s: %s",
                        email.c_str(), access_code.c_str());
                }
                else {
                    // SMTP failed, fall back to default code
                    access_code = tsto::token::Token::generate_access_code(user_id);
                    // Don't store access code in user_cred
                    logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] SMTP code generation failed, using fallback code for %s: %s",
                        email.c_str(), access_code.c_str());
                }
            }
            else {
                // Both verification methods are disabled, generate a proper access code
                access_code = tsto::token::Token::generate_access_code(user_id);
                // Don't store access code in user_cred
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Both SMTP and TSTO_API verification are disabled, using generated code: %s", access_code.c_str());
            }

            std::string existing_user_id;
            bool user_already_exists = db.get_user_id(filename, existing_user_id);

            if (user_already_exists) {
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] User with email %s already exists with ID %s, updating instead of creating new user",
                    filename.c_str(), existing_user_id.c_str());

                user_id = existing_user_id;

                std::string existing_mayhem_id = "";
                if (db.get_mayhem_id(filename, existing_mayhem_id) && !existing_mayhem_id.empty()) {
                    mayhem_id = existing_mayhem_id;
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Using existing mayhem ID %s for user %s",
                        mayhem_id.c_str(), filename.c_str());
                }

                if (!db.update_access_token(filename, access_token)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Failed to update access token for existing user: %s", filename.c_str());
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Successfully updated access token for existing user: %s", filename.c_str());

                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Using user ID %s and token %s",
                        user_id.c_str(), access_token.substr(0, 10).c_str());
                }

                if (!db.update_access_code(filename, access_code)) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Failed to update access code for existing user: %s", filename.c_str());
                }

                if (!user_cred.empty()) {
                    if (!db.update_user_cred(filename, user_cred)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to update user credential for existing user: %s", filename.c_str());
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Updated user credential for user: %s", filename.c_str());
                    }
                }

                std::string existing_display_name; // not working due to way i logged it out now
                if (!db.get_display_name(filename, existing_display_name) || existing_display_name.empty()) {
                    if (!db.update_display_name(filename, display_name)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to update display name for existing user: %s", filename.c_str());
                    }
                }
                else {
                    display_name = existing_display_name;
                }

                std::string existing_town_name;
                if (!db.get_town_name(filename, existing_town_name) || existing_town_name.empty()) {
                    if (!db.update_town_name(filename, town_name)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to update town name for existing user: %s", filename.c_str());
                    }
                }
                else {
                    town_name = existing_town_name;
                }

                std::string existing_device_id;
                if ((!db.get_device_id(filename, existing_device_id) || existing_device_id.empty()) && !device_id.empty()) {
                    if (!db.update_device_id(filename, device_id)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to update device ID for existing user: %s", filename.c_str());
                    }
                }
                else if (!existing_device_id.empty()) {
                    device_id = existing_device_id;
                }

                std::string existing_android_id;
                if ((!db.get_android_id(filename, existing_android_id) || existing_android_id.empty()) && !android_id.empty()) {
                    if (!db.update_android_id(filename, android_id)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to update Android ID for existing user: %s", filename.c_str());
                    }
                }
                else if (!existing_android_id.empty()) {
                    android_id = existing_android_id;
                }

                std::string existing_vendor_id;
                if ((!db.get_vendor_id(filename, existing_vendor_id) || existing_vendor_id.empty()) && !vendor_id.empty()) {
                    if (!db.update_vendor_id(filename, vendor_id)) {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to update Vendor ID for existing user: %s", filename.c_str());
                    }
                }
                else if (!existing_vendor_id.empty()) {
                    vendor_id = existing_vendor_id;
                }
            }
            else {

                std::string client_ip = std::string(ctx->remote_ip());
                

                if (!db.store_user_id(filename, user_id, access_token, mayhem_id, access_code, user_cred, display_name, town_name, "towns/" + filename + ".pb", device_id, android_id, vendor_id, client_ip, combined_id, advertising_id, platform_id, manufacturer, model, existing_session_key, existing_land_token, existing_uid, as_identifier)) {
                    throw std::runtime_error("Failed to store user data in database");
                }



                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Created new user and updated session with user ID %s and token %s",
                    user_id.c_str(), access_token.substr(0, 10).c_str());

                // Now verify that the API code was stored correctly if we used the API
                if (api_enabled && !user_cred.empty()) {
                    // Verify that the API code was actually stored
                    std::string stored_cred;
                    if (db.get_user_cred(email, stored_cred)) {
                        if (stored_cred == user_cred) {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Verified API code was correctly stored for user: %s", email.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] API code verification failed for user: %s (Stored: %s, Expected: %s)",
                                email.c_str(), stored_cred.c_str(), user_cred.c_str());
                        }
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Failed to retrieve stored API code for user: %s", email.c_str());
                    }
                }

                if (token_exists && existing_email != email) {
                    if (!as_identifier.empty()) {
                        if (!db.update_as_identifier(email, as_identifier)) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Failed to update AS identifier for email: %s", email.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Successfully updated AS identifier for email: %s", email.c_str());
                        }
                    }
                    
                    if (!existing_lnglv_token.empty()) {
                        if (!db.update_lnglv_token(email, existing_lnglv_token)) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Failed to update lnglv token for email: %s", email.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Successfully updated lnglv token for email: %s", email.c_str());
                        }
                    }

                    if (!existing_session_key.empty()) {
                        if (!db.update_session_key(email, existing_session_key)) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Failed to update session key for email: %s", email.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Successfully updated session key for email: %s", email.c_str());
                        }
                    }

                    if (!existing_land_token.empty()) {
                        if (!db.update_land_token(email, existing_land_token)) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Failed to update land token for email: %s", email.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Successfully updated land token for email: %s", email.c_str());
                        }
                    }

                    if (!existing_uid.empty()) {
                        if (!db.update_anon_uid(email, existing_uid)) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Failed to update anonymous UID for email: %s", email.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Successfully updated anonymous UID for email: %s", email.c_str());
                        }
                    }
                    
                    if (!as_identifier.empty()) {
                        if (!db.update_as_identifier(email, as_identifier)) {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Failed to update AS identifier for email: %s", email.c_str());
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Successfully updated AS identifier for email: %s", email.c_str());
                        }
                    }

                    if (transaction_started) {
                        if (db.commit_transaction()) {
                            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Successfully committed database transaction for user migration");
                        }
                        else {
                            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                                "[PROGREG CODE] Failed to commit database transaction for user migration");
                        }
                    }
                }
            }

            tsto::land::Land land;
            land.set_email(filename);
            if (!land.instance_load_town()) {
                std::string stored_town_path;
                bool has_existing_town = db.get_land_save_path(filename, stored_town_path) && !stored_town_path.empty();

                if (has_existing_town) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Found existing town file path in database for user: %s, path: %s",
                        filename.c_str(), stored_town_path.c_str());

                    if (std::filesystem::exists(stored_town_path)) {
                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Existing town file found at: %s, using it instead of creating a new one",
                            stored_town_path.c_str());

                        db.update_land_save_path(filename, stored_town_path);

                        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Successfully using existing town for user: %s", filename.c_str());
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
                            "[PROGREG CODE] Town file path exists in database but file not found, creating new town for user: %s",
                            filename.c_str());
                        create_new_town_for_user(filename);
                    }
                }
                else {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                        "[PROGREG CODE] Town not found for user: %s, creating blank town", filename.c_str());
                    create_new_town_for_user(filename);
                }
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[PROGREG CODE] Successfully loaded/created town for user: %s with filename: %s",
                filename.c_str(), land.get_town_filename().c_str());

            rapidjson::Document response;
            response.SetObject();
            rapidjson::Document::AllocatorType& allocator = response.GetAllocator();

            response.AddMember("code", rapidjson::Value(access_code.c_str(), allocator), allocator);
            response.AddMember("user_id", rapidjson::Value(user_id.c_str(), allocator), allocator);
            response.AddMember("mayhem_id", mayhem_id, allocator);

            rapidjson::StringBuffer buffer;
            rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
            response.Accept(writer);

            headers::set_json_response(ctx);
            cb(buffer.GetString());
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[PROGREG CODE] Error: %s", e.what());

            auto& db = tsto::database::Database::get_instance();
            if (transaction_started) {
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[PROGREG CODE] Rolling back database transaction due to error");
                db.rollback_transaction();
            }

            ctx->set_response_http_code(500);
            cb("");
        }
    }

    // Helper function to create a new town for a user without using session
    void Login::create_new_town_for_user(const std::string& filename) {
        // Get the town path for this user
        //std::string town_path = "towns/" + filename + ".pb";
        //
        //// Create a blank town using the new_land functionality that doesn't depend on session
        //if (!tsto::land::new_land::create_blank_town(town_path)) {
        //    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
        //        "[PROGREG CODE] Failed to create blank town for user: %s", filename.c_str());
        //    throw std::runtime_error("Failed to create blank town");
        //}

        //// Update the database with the town path
        //auto& db = tsto::database::Database::get_instance();
        //if (!db.update_land_save_path(filename, town_path)) {
        //    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
        //        "[PROGREG CODE] Failed to update town path in database for user: %s", filename.c_str());
        //}

        //logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
        //    "[PROGREG CODE] Successfully created blank town for user: %s at path: %s", 
        //    filename.c_str(), town_path.c_str());
    }

    static size_t curl_write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
        size_t newLength = size * nmemb;
        try {
            s->append((char*)contents, newLength);
            return newLength;
        }
        catch (std::bad_alloc& e) {
            return 0;
        }
    }

    std::string Login::handle_smtp(const std::string& email, const std::string& display_name) {
        bool smtp_enabled = utils::configuration::ReadBoolean("SMTP", "Enabled", false);

        if (!smtp_enabled) {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[SMTP] SMTP verification disabled");
            return "";
        }

        std::string smtp_server = utils::configuration::ReadString("SMTP", "Server", "");
        std::string smtp_port = utils::configuration::ReadString("SMTP", "Port", "");
        std::string smtp_username = utils::configuration::ReadString("SMTP", "Username", "");
        std::string smtp_password = utils::configuration::ReadString("SMTP", "Password", "");
        std::string smtp_from = utils::configuration::ReadString("SMTP", "From", "");

        if (smtp_server.empty() || smtp_username.empty() || smtp_password.empty() || smtp_from.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[SMTP] SMTP configuration incomplete");
            return "";
        }

        // Generate a random 6-digit code
        std::string verification_code;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(100000, 999999);
        verification_code = std::to_string(dis(gen));

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[SMTP] Generated verification code for %s: %s",
            email.c_str(), verification_code.c_str());

        try {
            CURL* curl = curl_easy_init();
            if (!curl) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[SMTP] Failed to initialize curl");
                return "";
            }

            struct curl_slist* recipients = NULL;
            recipients = curl_slist_append(recipients, email.c_str());

            std::string team_name = utils::configuration::ReadString("Server", "TeamName", "TSTO Server");
            std::string mail_body = "From: " + smtp_from + "\r\n"
                "To: " + email + "\r\n"
                "Subject: Your Verification Code for " + team_name + "\r\n"
                "\r\n"
                "Hello " + display_name + ",\r\n"
                "\r\n"
                "Your verification code is: " + verification_code + "\r\n"
                "\r\n"
                "This code will expire in 10 minutes.\r\n"
                "\r\n"
                "Regards,\r\n"
                + team_name + " Team";

            curl_easy_setopt(curl, CURLOPT_URL, ("smtp://" + smtp_server + ":" + smtp_port).c_str());
            curl_easy_setopt(curl, CURLOPT_USE_SSL, (long)CURLUSESSL_ALL);
            curl_easy_setopt(curl, CURLOPT_USERNAME, smtp_username.c_str());
            curl_easy_setopt(curl, CURLOPT_PASSWORD, smtp_password.c_str());
            curl_easy_setopt(curl, CURLOPT_MAIL_FROM, smtp_from.c_str());
            curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);
            curl_easy_setopt(curl, CURLOPT_READFUNCTION, NULL);
            curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);
            curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(curl, CURLOPT_READDATA, mail_body.c_str());
            curl_easy_setopt(curl, CURLOPT_INFILESIZE, mail_body.size());

            CURLcode res = curl_easy_perform(curl);

            curl_slist_free_all(recipients);
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[SMTP] curl_easy_perform() failed: %s", curl_easy_strerror(res));
                return "";
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[SMTP] Successfully sent verification code to %s", email.c_str());

            return verification_code;
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[SMTP] Exception while sending email: %s", e.what());
            return "";
        }
    }

    //ty ethan here 
    std::string Login::handle_tsto_api(const std::string& email, const std::string& display_name) {
        bool api_enabled = utils::configuration::ReadBoolean("TSTO_API", "Enabled", false);

        if (!api_enabled) {
            // If TSTO_API is disabled, return an empty string to indicate API is not available
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                "[TSTO_API] API verification disabled");
            return "";
        }

        //TSTO_API configuration from server config
        std::string api_key = utils::configuration::ReadString("TSTO_API", "ApiKey", "");
        std::string team_name = utils::configuration::ReadString("TSTO_API", "TeamName", "Bodnjenie");

        if (api_key.empty()) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[TSTO_API] API key not configured");
            return "";
        }

        auto url_encode = [](const std::string& value) -> std::string {
            std::ostringstream escaped;
            escaped.fill('0');
            escaped << std::hex;

            for (char c : value) {
                if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    escaped << c;
                }
                else if (c == ' ') {
                    escaped << '+';
                }
                else {
                    escaped << '%' << std::setw(2) << int((unsigned char)c);
                }
            }

            return escaped.str();
            };

        std::string username = display_name;
        std::string url = "https://api.tsto.app/api/auth/sendCode?apikey=" + url_encode(api_key) +
            "&emailAddress=" + url_encode(email) +
            "&teamName=" + url_encode(team_name) +
            "&username=" + url_encode(username);

        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[TSTO_API] Sending verification code to %s with URL: %s",
            email.c_str(), url.c_str());

        try {
            std::string response;

            CURL* curl = curl_easy_init();
            if (!curl) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[TSTO_API] Failed to initialize curl");
                return "";
            }

            //curl options
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POST, 1L); // Use POST method
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, ""); // Empty POST data, parameters are in URL
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // 10 seconds timeout
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Skip SSL verification for simplicity
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "Project Springfield/1.0");

            CURLcode res = curl_easy_perform(curl);

            // 
            curl_easy_cleanup(curl);

            if (res != CURLE_OK) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[TSTO_API] curl_easy_perform() failed: %s", curl_easy_strerror(res));
                return "";
            }

            if (response.empty()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[TSTO_API] Empty response from API");
                return "";
            }

            logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                "[TSTO_API] Raw API response: %s", response.c_str());

            rapidjson::Document doc;
            doc.Parse(response.c_str());

            if (doc.HasParseError() || !doc.IsObject()) {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[TSTO_API] Invalid JSON response from API: Parse error code %d",
                    doc.HasParseError() ? doc.GetParseError() : 0);
                return "";
            }

            for (auto it = doc.MemberBegin(); it != doc.MemberEnd(); ++it) {
                std::string key = it->name.GetString();
                std::string value = "unknown type";
                if (it->value.IsString()) {
                    value = it->value.GetString();
                }
                else if (it->value.IsInt()) {
                    value = std::to_string(it->value.GetInt());
                }
                else if (it->value.IsBool()) {
                    value = it->value.GetBool() ? "true" : "false";
                }
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[TSTO_API] Response field: %s = %s", key.c_str(), value.c_str());
            }

            if (doc.HasMember("code") && doc["code"].IsString()) {
                std::string code = doc["code"].GetString();
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[TSTO_API] Successfully received verification code from API for %s: %s",
                    email.c_str(), code.c_str());
                return code;
            }
            else if (doc.HasMember("code") && doc["code"].IsInt()) {
                std::string code = std::to_string(doc["code"].GetInt());
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_AUTH,
                    "[TSTO_API] Successfully received verification code (as int) from API for %s: %s",
                    email.c_str(), code.c_str());
                return code;
            }
            else {
                logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                    "[TSTO_API] No code in API response");
                return "";
            }
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_AUTH,
                "[TSTO_API] Exception while calling API: %s", e.what());
            return "";
        }
    }

}
