#pragma once
#include <std_include.hpp>
#include "cryptography.hpp"
#include <random>
#include <chrono>
#include <string.hpp>

#include "debugging/serverlog.hpp"
#include "LandData.pb.h"

namespace tsto {
    class Session {
    public:
        static Session& get() {
            static Session instance;
            return instance;
        }

        Session(const Session&) = delete;
        Session& operator=(const Session&) = delete;
        Session(Session&&) = delete;
        Session& operator=(Session&&) = delete;

        std::string session_key;
        std::string land_token;
        std::string user_user_id;
        std::string user_telemetry_id;
        std::string token_session_key;
        std::string token_user_id;
        int64_t token_authenticator_pid_id;
        int64_t token_persona_id;
        std::string token_telemetry_id;
        std::string personal_id;
        std::string display_name;
        std::string persona_name;
        std::string town_filename;

        std::string me_persona_id;
        std::string me_persona_pid_id;
        std::string me_persona_display_name;
        std::string me_persona_name;
        std::string me_persona_anonymous_id;
        std::string me_display_name;
        std::string me_anonymous_id;

        std::string device_id;

        std::string lnglv_token;     
        std::string access_token;    

        Data::LandMessage land_proto;

        // Reinitialize the session
        void reinitialize() {
            initialize();
        }

    private:

        Session() {
            initialize();
        }


        std::string generate_random_digits(size_t length) {
            std::string result;
            result.reserve(length);
            for (size_t i = 0; i < length; i++) {
                result += std::to_string(utils::cryptography::random::get_integer() % 10);
            }
            return result;
        }

        std::string generate_random_name(size_t min_length, size_t max_length) {
            static const std::string chars =
                "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

            size_t length = min_length + (utils::cryptography::random::get_integer() % (max_length - min_length + 1));
            std::string result;
            result.reserve(length);

            for (size_t i = 0; i < length; i++) {
                result += chars[utils::cryptography::random::get_integer() % chars.length()];
            }
            return result;
        }

        int64_t generate_random_id() {
            return 1000000000000 + (utils::cryptography::random::get_longlong() % 98999999999999);
        }

        void initialize() {
            // session
            session_key = utils::cryptography::random::get_challenge();

            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] session_key: %s", session_key.c_str());

            // land
            land_token = utils::cryptography::random::get_challenge();

            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] land_token: %s", land_token.c_str());

            // user
            user_user_id = generate_random_digits(38);
            user_telemetry_id = generate_random_digits(11);

            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] user_user_id: %s", user_user_id.c_str());
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] user_telemetry_id: %s", user_telemetry_id.c_str());

            // MD5 based on current time
            auto now = std::chrono::system_clock::now();
            auto now_str = std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()).count());
            token_session_key = utils::cryptography::md5::compute(now_str, true);

            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] token_session_key: %s", token_session_key.c_str());

            // token
            token_user_id = /*"90159726165211658982621159447878257465"*/std::to_string(generate_random_id());
            token_authenticator_pid_id = generate_random_id();
            token_persona_id = generate_random_id();
            token_telemetry_id = std::to_string(generate_random_id());
            personal_id = std::to_string(generate_random_id());

            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] token_user_id: %s", token_user_id.c_str());
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] token_authenticator_pid_id: %llu", token_authenticator_pid_id);
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] token_persona_id: %llu", token_persona_id);
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] token_telemetry_id: %s", token_telemetry_id.c_str());
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] personal_id: %s", personal_id.c_str());

            // persona
            me_persona_name = utils::string::to_lower(me_persona_display_name);
            persona_name = utils::string::to_lower(display_name);
            me_persona_id = std::to_string(generate_random_id());
            me_persona_pid_id = std::to_string(generate_random_id());
            me_persona_display_name = generate_random_name(5, 12);
            me_persona_name = utils::string::to_lower(me_persona_display_name);
            me_persona_anonymous_id = utils::cryptography::base64::encode(
                utils::cryptography::md5::compute(me_persona_name));
            me_display_name = me_persona_display_name;
            me_anonymous_id = me_persona_anonymous_id;

            lnglv_token = "QVQwOjIuMDozLjA6ODY0MDA6S3BuUUdaTzJTSXhEMHhLWkVMOXBCYXVWZEJKWVJPME5ZRDI6NDcwODI6cmlxYWM";
			access_token = "";  // shud be set from headers later

            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] me_persona_name: %s", me_persona_name.c_str());
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] persona_name: %s", persona_name.c_str());
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] me_persona_id: %s", me_persona_id.c_str());
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] me_persona_pid_id: %s", me_persona_pid_id.c_str());
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] me_persona_display_name: %s", me_persona_display_name.c_str());
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] me_persona_anonymous_id: %s", me_persona_anonymous_id.c_str());

            // device ID (SHA256 of current time)
            device_id = utils::cryptography::sha256::compute(now_str, true);
            logger::write(logger::LOG_LEVEL_PLAYER_ID, logger::LOG_LABEL_SESSION, "[INIT] device_id: %s", device_id.c_str());
        }

    };
}