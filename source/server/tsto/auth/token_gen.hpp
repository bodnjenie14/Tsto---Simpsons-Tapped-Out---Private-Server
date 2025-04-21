#pragma once

#include <evpp/http/context.h>
#include <evpp/event_loop.h>
#include <string>
#include "debugging/serverlog.hpp"
#include "headers/response_headers.hpp"
#include "tsto/includes/session.hpp"

namespace tsto::token {

    class Token {
    public:
        static std::string url_decode(const std::string& encoded);
        static std::string generate_random_string(size_t length);
        
        static std::string generate_random_token(const std::string& user_id);
        static std::string generate_access_token(const std::string& user_id);
        static std::string generate_typed_access_token(const std::string& type, const std::string& user_id);
        static std::string generate_random_code();
        static std::string generate_access_code(const std::string& user_id);
        
        static bool extract_user_id_from_token(const std::string& token, std::string& user_id);
        
        static std::string generate_temp_user();
        static bool generate_temp_user(const std::string& device_id, std::string& email, std::string& user_id, 
                                      std::string& access_token, const std::string& android_id = "", 
                                      const std::string& vendor_id = "");
        
        static bool verify_device_id_uniqueness(const std::string& device_id, const std::string& exclude_email = "");
    };
}