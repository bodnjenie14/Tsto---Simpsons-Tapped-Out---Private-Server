#include <std_include.hpp>
#include "file_server.hpp"
#include <fstream>
#include <sstream>
#include "debugging/serverlog.hpp"

namespace file_server {
    FileServer::FileServer() {
        base_directory_ = "dlc";  // Simple relative path, just like in Land class

        try {
            if (!std::filesystem::exists(base_directory_)) {
                std::filesystem::create_directories(base_directory_);
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                    "Created DLC directory: %s", base_directory_.c_str());
            }

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                "DLC directory set to: %s", base_directory_.c_str());
        }
        catch (const std::filesystem::filesystem_error& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                "Failed to create DLC directory: %s", e.what());
        }
    }

    void FileServer::handle_dlc_download(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {
        try {
            std::string uri = ctx->uri();

            // Remove "/static/" prefix
            if (uri.find("/static/") == 0) {
                uri = uri.substr(8);  // Length of "/static/"
            }

            // Sanitize filename
            uri = sanitize_filename(uri);

            // Construct file path without double slashes
            std::string file_path = "dlc/" + uri;  // All static content goes in dlc directory

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                "Attempting to serve static file: %s", file_path.c_str());

            // Check if file exists and serve it
            if (std::filesystem::exists(file_path)) {
                std::ifstream file(file_path, std::ios::binary);
                std::stringstream buffer;
                buffer << file.rdbuf();

                ctx->AddResponseHeader("Content-Type", "application/zip");
                cb(buffer.str());

                // Log successful file serve
                logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                    "Successfully served static file: %s (Size: %zu bytes)", file_path.c_str(), buffer.str().size());
            }
            else {
                logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_FILESERVER,
                    "Static file not found: %s", file_path.c_str());
                ctx->set_response_http_code(404);
                cb("File not found");
            }
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                "Error serving static file: %s", e.what());
            ctx->set_response_http_code(500);
            cb("Internal server error");
        }
    }

    bool FileServer::is_path_safe(const std::string& requested_path) const {
        try {
            std::filesystem::path req_path = requested_path;
            std::filesystem::path base = base_directory_;

            auto canonical_req = std::filesystem::weakly_canonical(req_path);
            auto canonical_base = std::filesystem::weakly_canonical(base);

            auto req_str = canonical_req.string();
            auto base_str = canonical_base.string();

            std::replace(req_str.begin(), req_str.end(), '\\', '/');
            std::replace(base_str.begin(), base_str.end(), '\\', '/');

            return req_str.find(base_str) == 0;
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                "Path safety check failed: %s", e.what());
            return false;
        }
    }

    std::string FileServer::sanitize_filename(const std::string& filename) const {
        std::string result = filename;
        const std::string allowed_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_./";

        for (auto& c : result) {
            if (allowed_chars.find(c) == std::string::npos) {
                c = '_';
            }
        }

        return result;
    }
}