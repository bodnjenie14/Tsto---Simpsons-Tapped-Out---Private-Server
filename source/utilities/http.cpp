#include "http.hpp"
#include <sstream>
#include <iomanip>

namespace utils::http
{
        namespace
        {
                std::vector<std::string> split_string(const std::string& str, char delimiter)
                {
                        std::vector<std::string> tokens;
                        std::string token;
                        std::istringstream token_stream(str);
                        while (std::getline(token_stream, token, delimiter))
                        {
                                if (!token.empty() && token != "\r")
                                {
                                        tokens.push_back(token);
                                }
                        }
                        return tokens;
                }

                bool starts_with(const std::string& str, const std::string& prefix)
                {
                    return str.length() >= prefix.length() && 
                           str.compare(0, prefix.length(), prefix) == 0;
                }
        }

        request_method HttpLite::get_http_request_method(const std::string& method)
        {
            if (method == "GET") return request_method::GET;
            if (method == "POST") return request_method::POST;
            if (method == "PUT") return request_method::PUT;
            if (method == "DELETE") return request_method::DELETE;
            if (method == "HEAD") return request_method::HEAD;
            if (method == "OPTIONS") return request_method::OPTIONS;
            return request_method::UNKNOWN;
        }

        transfer_encoding HttpLite::get_transfer_encoding(const std::string& name)
        {
            if (name == "chunked") return transfer_encoding::CHUNKED;
            if (name == "compress") return transfer_encoding::COMPRESS;
            if (name == "deflate") return transfer_encoding::DEFLATE;
            if (name == "gzip") return transfer_encoding::GZIP;
            if (name == "identity") return transfer_encoding::IDENTITY;
            return transfer_encoding::NONE;
        }

        bool HttpLite::GetDataChunk(std::string& output)
        {
            if (chunk_start_ >= body.length()) return false;

            size_t chunk_end = body.find("\r\n", chunk_start_);
            if (chunk_end == std::string::npos) return false;

            // Parse chunk size
            std::string chunk_size_str = body.substr(chunk_start_, chunk_end - chunk_start_);
            size_t chunk_size;
            std::istringstream(chunk_size_str) >> std::hex >> chunk_size;

            if (chunk_size == 0) return false;

            // Skip CRLF after size
            chunk_start_ = chunk_end + 2;

            // Extract chunk data
            output = body.substr(chunk_start_, chunk_size);
            chunk_start_ += chunk_size + 2; // Skip CRLF after chunk

            return true;
        }

        std::string HttpLite::GetRequestPath()
        {
            return uri::get_path(uri);
        }

        bool HttpLite::FindQueryRequest(std::string key, std::string& output)
        {
            output = uri::get_query(uri, key);
            return !output.empty();
        }

        bool HttpLite::ParseRequest(const std::string& request)
        {
                auto lines = split_string(request, '\n');
                if (lines.empty()) return false;

                // Parse request line
                auto parts = split_string(lines[0], ' ');
                if (parts.size() != 3) return false;

                method = parts[0];
                uri = parts[1];
                version = parts[2];

                // Parse headers
                headers.clear();
                for (size_t i = 1; i < lines.size(); ++i)
                {
                        auto& line = lines[i];
                        if (line.empty() || line == "\r") break;

                        auto pos = line.find(':');
                        if (pos == std::string::npos) continue;

                        auto key = line.substr(0, pos);
                        auto value = line.substr(pos + 1);
                        while (!value.empty() && (value[0] == ' ' || value[0] == '\r'))
                        {
                                value.erase(0, 1);
                        }
                        while (!value.empty() && value.back() == '\r')
                        {
                                value.pop_back();
                        }

                        headers[key] = value;
                }

                return true;
        }

        std::string HttpLite::GetHeader(const std::string& key) const
        {
                auto it = headers.find(key);
                return (it != headers.end()) ? it->second : "";
        }

        void HttpLite::SetHeader(const std::string& key, const std::string& value)
        {
                headers[key] = value;
        }

        std::string HttpLite::BuildResponse() const
        {
                std::ostringstream response;
                response << version << " " << status_code << " " << status_text << "\r\n";

                for (const auto& header : headers)
                {
                        response << header.first << ": " << header.second << "\r\n";
                }

                response << "\r\n" << body;
                return response.str();
        }

        std::string url_encode(const std::string& data)
        {
                std::ostringstream encoded;
                encoded.fill('0');
                encoded << std::hex;

                for (char c : data)
                {
                        if (std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_' || c == '.' || c == '~')
                        {
                                encoded << c;
                        }
                        else
                        {
                                encoded << '%' << std::setw(2) << std::uppercase 
                                      << static_cast<int>(static_cast<unsigned char>(c));
                        }
                }

                return encoded.str();
        }

        std::string url_decode(const std::string& data)
        {
                std::string result;
                result.reserve(data.length());

                for (size_t i = 0; i < data.length(); ++i)
                {
                        if (data[i] == '%' && i + 2 < data.length())
                        {
                                int value;
                                std::istringstream hex_chars(data.substr(i + 1, 2));
                                hex_chars >> std::hex >> value;
                                result += static_cast<char>(value);
                                i += 2;
                        }
                        else if (data[i] == '+')
                        {
                                result += ' ';
                        }
                        else
                        {
                                result += data[i];
                        }
                }

                return result;
        }

        namespace header
        {
                std::string get_server_time()
                {
                        time_t now = time(nullptr);
                        struct tm tm_time;
#ifdef _WIN32
                        gmtime_s(&tm_time, &now);
#else
                        gmtime_r(&now, &tm_time);
#endif
                        char buffer[128];
                        strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", &tm_time);
                        return std::string(buffer);
                }

                const char* get_content_type(const char* ext)
                {
                        if (!ext || ext[0] == '\0') return "application/octet-stream";

                        static const std::map<std::string, const char*> mime_types = {
                                {"html", "text/html"},
                                {"htm", "text/html"},
                                {"css", "text/css"},
                                {"js", "application/javascript"},
                                {"json", "application/json"},
                                {"png", "image/png"},
                                {"jpg", "image/jpeg"},
                                {"jpeg", "image/jpeg"},
                                {"gif", "image/gif"},
                                {"svg", "image/svg+xml"},
                                {"ico", "image/x-icon"}
                        };

                        auto it = mime_types.find(ext);
                        return (it != mime_types.end()) ? it->second : "application/octet-stream";
                }
        }

        namespace uri
        {
                std::string get_path(const std::string& uri)
                {
                        size_t qmark_pos = uri.find('?');
                        return (qmark_pos != std::string::npos) ? uri.substr(0, qmark_pos) : uri;
                }

                std::string get_query(const std::string& uri, std::string key)
                {
                        size_t qmark_pos = uri.find('?');
                        if (qmark_pos == std::string::npos) return "";

                        std::string query = uri.substr(qmark_pos + 1);
                        key += "=";
                        size_t key_pos = query.find(key);
                        if (key_pos == std::string::npos) return "";

                        size_t value_start = key_pos + key.length();
                        size_t value_end = query.find('&', value_start);
                        if (value_end == std::string::npos) value_end = query.length();

                        return query.substr(value_start, value_end - value_start);
                }
        }
}
