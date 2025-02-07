#pragma once

#include <string>
#include <vector>

namespace utils::http
{
    enum request_method {
        HTTP_REQ_NONE = 0,
        HTTP_REQ_GET = 1,
        HTTP_REQ_POST = 2,
        HTTP_REQ_PUT = 3,
        HTTP_REQ_UNKNOWN = -1,
    };

    enum transfer_encoding {
        HTTP_TE_NONE = 0,
        HTTP_TE_CHUNKED = 1,
        HTTP_TE_COMPRESS = 2,
        HTTP_TE_DEFLATE = 3,
        HTTP_TE_GZIP = 3,
        HTTP_TE_UNKNOWN = -1,
    };


    class HttpLite final
    {
    public:
        std::string header;
        std::string content;
        bool headerExists = false;

        std::string req_uri;
        std::string host_name;
        request_method req_method{};
        transfer_encoding transfer_enc{};

        bool ParseRequest(const std::string& request);

        bool GetDataChunk(std::string& output);

        std::string GetRequestPath();
        bool FindQueryRequest(std::string key, std::string& output);


    private:
        request_method get_http_request_method(const std::string& method);
        transfer_encoding get_transfer_encoding(const std::string& name);

    };

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
