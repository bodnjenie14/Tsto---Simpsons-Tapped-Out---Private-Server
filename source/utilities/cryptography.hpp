#pragma once

#include <string>
#include <tommath.h>
#include <tomcrypt.h>

#define LTM_DESC

namespace utils::cryptography
{
    namespace sha256
    {
        std::string compute(const std::string& data, bool hex = false);
        std::string compute(const uint8_t* data, size_t length, bool hex = false);
    }

    namespace md5
    {
        std::string compute(const std::string& data, bool hex = false);
        std::string compute(const uint8_t* data, size_t length, bool hex = false);
    }

    namespace base64
    {
        std::string encode(const uint8_t* data, size_t len);
        std::string encode(const std::string& data);
        std::string decode(const std::string& data);
    }

    namespace random
    {
        uint32_t get_integer();
        uint64_t get_longlong();
        std::string get_challenge();
        void get_data(void* data, size_t size);
    }
}
