#pragma once

#include <string>
#include <vector>
#include <map>

namespace utils::http
{
    enum class request_method {
        NONE = 0,
        GET,
        POST,
        PUT,
        DELETE,
        HEAD,
        OPTIONS,
        UNKNOWN
    };

    enum class transfer_encoding {
        NONE = 0,
        CHUNKED,
        COMPRESS,
        DEFLATE,
        GZIP,
        IDENTITY
    };

    class HttpLite final
    {
    public:
        bool ParseRequest(const std::string& request);
        std::string GetHeader(const std::string& key) const;
        void SetHeader(const std::string& key, const std::string& value);
        std::string BuildResponse() const;

        // Additional methods
        request_method get_http_request_method(const std::string& method);
        transfer_encoding get_transfer_encoding(const std::string& name);
        bool GetDataChunk(std::string& output);
        std::string GetRequestPath();
        bool FindQueryRequest(std::string key, std::string& output);

        // Request data
        std::string method;
        std::string uri;
        std::string version;
        std::map<std::string, std::string> headers;
        std::string body;

        // Response data
        int status_code{200};
        std::string status_text{"OK"};

    private:
        size_t chunk_start_{0};
    };

    // URL encoding/decoding
    std::string url_encode(const std::string& data);
    std::string url_decode(const std::string& data);

    namespace header
    {
        std::string get_server_time();
        const char* get_content_type(const char* ext);
    }

    namespace uri
    {
        std::string get_path(const std::string& uri);
        std::string get_query(const std::string& uri, std::string key);
    }
}
