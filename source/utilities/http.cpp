#include "http.hpp"
#include <format>
#include <time.h>
#include "string.hpp"

namespace utils::http
{
    request_method HttpLite::get_http_request_method(const std::string& method)
    {
        if (method == "GET") {
            return HTTP_REQ_GET;
        }
        else if (method == "PUT") {
            return HTTP_REQ_PUT;
        }
        else if (method == "POST") {
            return HTTP_REQ_POST;
        }
        else {
            return HTTP_REQ_UNKNOWN;
        }
    }

    transfer_encoding HttpLite::get_transfer_encoding(const std::string& name)
    {
        if (name == "chunked") {
            return HTTP_TE_CHUNKED;
        }
        else if (name == "compress") {
            return HTTP_TE_COMPRESS;
        }
        else if (name == "deflate") {
            return HTTP_TE_DEFLATE;
        }
        else if (name == "gzip") {
            return HTTP_TE_GZIP;
        }
        else {
            return HTTP_TE_UNKNOWN;
        }
    }

    bool HttpLite::GetDataChunk(std::string& output)
    {
        auto line_end = this->content.find("\r\n"); // 0x0D 0x0A
        if (line_end == std::string::npos) return false;

        //  [BEGIN] 13B0 <--> \r\n <--> buffer <--> \r\n [END]

        std::string lengthX16 = this->content.substr(0, line_end);
        size_t chunk_length = std::stoul(lengthX16, nullptr, 16);

        // buffer could overrun single tcp packet size 
        //size_t logical_size = line_end + 2 + chunk_length + 2;
        //if (this->content.size() != logical_size) return false;

        output = this->content.substr(line_end + 2, chunk_length);

        return true;
    }

    std::string HttpLite::GetRequestPath()
    {
        size_t qmark_pos = this->req_uri.find("?");

        return (qmark_pos != std::string::npos ? this->req_uri.substr(0, qmark_pos) : this->req_uri);
    }

    bool HttpLite::FindQueryRequest(std::string key, std::string& output)
    {
        auto uri_segments = string::split(this->req_uri, "/");
        auto& last_segment = uri_segments[uri_segments.size() - 1];

        size_t key_pos = last_segment.find(key.append("="));
        if (key_pos != std::string::npos)
        {
            size_t start_pos = key_pos + key.size();
            size_t finish_pos = last_segment.find("&", start_pos);
            output = last_segment.substr(start_pos, finish_pos - start_pos);

            return true;
        }

        return false;
    }

    bool HttpLite::ParseRequest(const std::string& request)
    {
        auto ridge = request.find("\r\n\r\n");
        if (ridge != std::string::npos && request[0] != 0x30) {
            this->header = request.substr(0, ridge);
            this->content = request.substr(ridge + 4, std::string::npos);

            std::vector<std::string> lines = string::split(this->header, "\r\n");
            std::vector<std::string> splitter = string::split(lines[0], ' ');

            if (!splitter[2].starts_with("HTTP")) return false;

            this->req_uri = splitter[1];
            this->req_method = get_http_request_method(splitter[0]);

            //for (size_t i = 1; i < lines.size(); i++)
            //{
            //    std:
            //}
        }
        else {
            this->content = request;
        }

        return true;
    }

    //class HTTPURL /* http://www.zedwood.com/article/cpp-boost-url-regex */
    //{
    //private:
    //    std::string _protocol;// http vs https
    //    std::string _domain;  // mail.google.com
    //    uint16_t _port;  // 80,443
    //    std::string _path;    // /mail/
    //    std::string _query;   // [after ?] a=b&c=b

    //public:
    //    const std::string& protocol;
    //    const std::string& domain;
    //    const uint16_t& port;
    //    const std::string& path;
    //    const std::string& query;

    //    HTTPURL(const std::string& url) : protocol(_protocol), domain(_domain), port(_port), path(_path), query(_query)
    //    {
    //        std::string u = _trim(url);
    //        size_t offset = 0, slash_pos, hash_pos, colon_pos, qmark_pos;
    //        std::string urlpath, urldomain, urlport;
    //        uint16_t default_port;

    //        static const char* allowed[] = { "https://", "http://", "ftp://", NULL };
    //        for (int i = 0; allowed[i] != NULL && this->_protocol.length() == 0; i++)
    //        {
    //            const char* c = allowed[i];
    //            if (u.compare(0, strlen(c), c) == 0) {
    //                offset = strlen(c);
    //                this->_protocol = std::string(c, 0, offset - 3);
    //            }
    //        }
    //        default_port = this->_protocol == "https" ? 443 : 80;
    //        slash_pos = u.find_first_of('/', offset + 1);
    //        urlpath = slash_pos == std::string::npos ? "/" : u.substr(slash_pos);
    //        urldomain = std::string(u.begin() + offset, slash_pos != std::string::npos ? u.begin() + slash_pos : u.end());
    //        urlpath = (hash_pos = urlpath.find("#")) != std::string::npos ? urlpath.substr(0, hash_pos) : urlpath;
    //        urlport = (colon_pos = urldomain.find(":")) != std::string::npos ? urldomain.substr(colon_pos + 1) : "";
    //        urldomain = urldomain.substr(0, colon_pos != std::string::npos ? colon_pos : urldomain.length());
    //        this->_domain = _ttolower(urldomain);
    //        this->_query = (qmark_pos = urlpath.find("?")) != std::string::npos ? urlpath.substr(qmark_pos + 1) : "";
    //        this->_path = qmark_pos != std::string::npos ? urlpath.substr(0, qmark_pos) : urlpath;
    //        this->_port = urlport.length() == 0 ? default_port : _atoi(urlport);
    //    };
    //private:
    //    static inline std::string _trim(const string& input)
    //    {
    //        std::string str = input;
    //        size_t endpos = str.find_last_not_of(" \t\n\r");
    //        if (std::string::npos != endpos)
    //        {
    //            str = str.substr(0, endpos + 1);
    //        }
    //        size_t startpos = str.find_first_not_of(" \t\n\r");
    //        if (std::string::npos != startpos)
    //        {
    //            str = str.substr(startpos);
    //        }
    //        return str;
    //    };
    //    static inline std::string _ttolower(const std::string& input)
    //    {
    //        std::string str = input;
    //        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
    //        return str;
    //    };
    //    static inline int _atoi(const std::string& input)
    //    {
    //        int r;
    //        std::stringstream(input) >> r;
    //        return r;
    //    };
    //};

	std::string header::get_server_time()
	{
		char date[64];
		const auto now = time(nullptr);
		tm gmtm{};
		gmtime_s(&gmtm, &now);
		strftime(date, 64, "%a, %d %b %G %T", &gmtm);

		return std::format("{} GMT", date);
	}

    const char* header::get_content_type(const char* ext)
    {
        if (ext == nullptr) {
            return "type/invalid";
        }
        else if (strlen(ext) == 0) {
            return "text/plain";
        }
        else if (!strcmp(ext, "html")) {
            return "text/html";
        }
        else if (!strcmp(ext, "css")) {
            return "text/css";
        }
        else if (!strcmp(ext, "js")) {
            return "application/javascript";
        }
        else if (!strcmp(ext, "jpg")) {
            return "image/jpeg";
        }
        else if (!strcmp(ext, "png")) {
            return "image/png";
        }
        else if (!strcmp(ext, "ico")) {
            return "image/x-icon";
        }
        else if (!strcmp(ext, "json")) {
            return "application/json";
        }
        else {
            return "type/unknown";
        }
    }

	std::string uri::get_query(const std::string& uri, std::string key)
	{
		auto uri_segments = string::split(uri,"/");
		auto& last_segment = uri_segments[uri_segments.size() - 1];

		size_t key_pos = last_segment.find(key.append("="));
		if (key_pos != std::string::npos)
		{
			size_t start_pos = key_pos + key.size();
			size_t finish_pos = last_segment.find("&", start_pos);
			return last_segment.substr(start_pos, finish_pos - start_pos);
		}

		return "";
	}

    std::string uri::get_path(const std::string& uri)
    {
        size_t qmark_pos = uri.find("?");

        return (qmark_pos != std::string::npos ? uri.substr(0, qmark_pos) : uri);
    }
}
