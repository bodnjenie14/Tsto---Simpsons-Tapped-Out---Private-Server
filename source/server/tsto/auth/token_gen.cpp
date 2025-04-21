#include <std_include.hpp>
#include "token_gen.hpp"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include "tsto_server.hpp"
#include <serialization.hpp>
#include "tsto/includes/session.hpp" 
#include <AuthData.pb.h> 
#include <sstream>
#include <random>
#include "tsto/database/database.hpp"
#include "tsto/land/new_land.hpp"
#include <vector>
#include <cctype> 
#include <algorithm> 
#include <filesystem>
#include "configuration.hpp"

namespace tsto::token{

    std::string Token::url_decode(const std::string& encoded) {
        std::string result;
        result.reserve(encoded.length());

        for (size_t i = 0; i < encoded.length(); ++i) {
            if (encoded[i] == '%' && i + 2 < encoded.length()) {
                int value = 0;
                std::istringstream is(encoded.substr(i + 1, 2));
                if (is >> std::hex >> value) {
                    result += static_cast<char>(value);
                    i += 2;
                }
                else {
                    result += encoded[i];
                }
            }
            else if (encoded[i] == '+') {
                result += ' ';
            }
            else {
                result += encoded[i];
            }
        }

        return result;
    }

    //gen random string
    std::string Token::generate_random_string(size_t length) {
        static const char charset[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
        std::string result;
        result.resize(length);

        for (size_t i = 0; i < length; ++i) {
            result[i] = charset[rand() % (sizeof(charset) - 1)];
        }

        return result;
    }

    //gen random token
    std::string Token::generate_random_token(const std::string& user_id) {
        std::string random_string = Token::generate_random_string(32);
        return random_string;
    }

    std::string Token::generate_access_token(const std::string& user_id) {
        std::string random_part = generate_random_string(32);
        std::string random_suffix = generate_random_string(5);

        return "AT0:2.0:3.0:86400:" + random_part + ":" + user_id + ":" + random_suffix;
    }

    std::string Token::generate_random_code() {
        return generate_random_string(40); // 40 char random string
    }

    std::string Token::generate_typed_access_token(const std::string& type, const std::string& user_id) {
        std::string formatted_user_id = user_id;
        
        if (formatted_user_id.length() != 13) {
            if (formatted_user_id.find_first_not_of("0123456789") == std::string::npos) {
                formatted_user_id = std::string(13 - formatted_user_id.length(), '0') + formatted_user_id;
                logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
                    "[TOKEN] Padded numeric user ID %s to 13-digit format: %s", 
                    user_id.c_str(), formatted_user_id.c_str());
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_AUTH,
                    "[TOKEN] User ID %s is not in the expected 13-digit format and cannot be padded", 
                    user_id.c_str());
            }
        }
        
        std::stringstream ss;
        ss << type << "0:2.0:3.0:86400:";
        ss << generate_random_string(10); // 10 char random string (was 32)
        ss << ":" << formatted_user_id << ":MPDON"; // Add MPDON suffix
        
        std::string token = ss.str();
        logger::write(logger::LOG_LEVEL_DEBUG, logger::LOG_LABEL_AUTH,
            "[TOKEN] Generated token: %s with user ID: %s", 
            token.c_str(), formatted_user_id.c_str());
        
        return token;
    }

    std::string Token::generate_access_code(const std::string& user_id) {
        return Token::generate_typed_access_token("AC", user_id);
    }

}