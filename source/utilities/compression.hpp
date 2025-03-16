#pragma once
#include <string>
#include <unordered_map>

#define CHUNK 16384u

namespace utils::compression
{
    namespace zlib
    {
        std::string compress(const std::string& data);
        std::string decompress(const std::string& data, bool is_gzip = false);
        std::string decompress_gzip(const std::string& data);
    }

    namespace zip
    {
        class archive final
        {
        public:
            archive() = default;
            ~archive() = default;
            
            // Move-only
            archive(archive&&) = default;
            archive& operator=(archive&&) = default;
            
            // No copying
            archive(const archive&) = delete;
            archive& operator=(const archive&) = delete;

            void add(std::string filename, std::string data);
            bool write(const std::string& filename, const std::string& comment = {});

        private:
            std::unordered_map<std::string, std::string> files_;
        };

        std::unordered_map<std::string, std::string> extract(const std::string& data);
    }
}
