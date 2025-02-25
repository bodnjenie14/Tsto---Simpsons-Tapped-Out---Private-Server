#include <std_include.hpp>
#include "file_server.hpp"
#include <fstream>
#include <sstream>
#include "debugging/serverlog.hpp"
#include <mutex>
#include <thread>
#include <evpp/event_loop.h>
#include "configuration.hpp"
namespace file_server {
    FileServer::FileServer() : file_mutex_(), queue_mutex_() {
        std::lock_guard<std::mutex> lock(file_mutex_);
        
        base_directory_ = utils::configuration::ReadString("Server", "DLCDirectory", "");

        if (base_directory_.empty()) {
            base_directory_ = "dlc";
            utils::configuration::WriteString("Server", "DLCDirectory", base_directory_);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                "Setting default DLC directory: %s", base_directory_.c_str());
        } else {
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                "Using configured DLC directory: %s", base_directory_.c_str());
        }

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

    void FileServer::cleanup_completed_ops() {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        while (!pending_ops_.empty()) {
            auto& op = pending_ops_.front();
            if (op.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                pending_ops_.pop();
            }
            else {
                break;
            }
        }
    }

    void FileServer::async_read_file(evpp::EventLoop* loop, const std::string& file_path,
        const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {

        auto future = std::async(std::launch::async, [this, file_path, ctx, cb, loop]() {
            try {
                std::string response_data;
                {
                    std::lock_guard<std::mutex> lock(file_mutex_);
                    if (std::filesystem::exists(file_path)) {
                        std::ifstream file(file_path, std::ios::binary);
                        std::stringstream buffer;
                        buffer << file.rdbuf();
                        response_data = buffer.str();
                    }
                }

                loop->RunInLoop([response_data, ctx, cb, file_path]() {
                    if (!response_data.empty()) {
                        ctx->AddResponseHeader("Content-Type", "application/zip");
                        cb(response_data);
#ifdef DEBUG
                        //logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                        //   "Successfully served static file: %s (Size: %zu bytes)", 
                        //   file_path.c_str(), response_data.size());
#endif
                    }
                    else {
                        logger::write(logger::LOG_LEVEL_WARN, logger::LOG_LABEL_FILESERVER,
                            "Static file not found: %s", file_path.c_str());
                        ctx->set_response_http_code(404);
                        cb("File not found");
                    }
                    });
            }
            catch (const std::exception& e) {
                loop->RunInLoop([ctx, cb, e]() {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                        "Error serving static file: %s", e.what());
                    ctx->set_response_http_code(500);
                    cb("Internal server error");
                    });
            }
            });

        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            pending_ops_.push(std::move(future));
        }
    }

    void FileServer::handle_dlc_download(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        cleanup_completed_ops();  // Clean up completed operations

        try {
            std::string uri = ctx->uri();

            // Remove "/static/" prefix
            if (uri.find("/static/") == 0) {
                uri = uri.substr(8);  // Length of "/static/"
            }

            uri = sanitize_filename(uri);

            std::string file_path;
            if (std::filesystem::path(base_directory_).is_absolute()) {
                file_path = base_directory_ + "/" + uri;
            } else {
                file_path = "dlc/" + uri;
            }

#ifdef DEBUG
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
               "Attempting to serve static file: %s", file_path.c_str());
#endif

            async_read_file(loop, file_path, ctx, cb);
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                "Error in handle_dlc_download: %s", e.what());
            ctx->set_response_http_code(500);
            cb("Internal server error");
        }
    }

    bool FileServer::is_path_safe(const std::string& requested_path) const {
        try {
            // Allow any path as long as it exists
            return std::filesystem::exists(requested_path);
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

    std::queue<std::future<void>> pending_ops_;
}