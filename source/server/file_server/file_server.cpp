#include <std_include.hpp>
#include "file_server.hpp"
#include <fstream>
#include <sstream>
#include "debugging/serverlog.hpp"
#include <mutex>
#include <thread>
#include <evpp/event_loop.h>
#include "configuration.hpp"
#include <ctime>
#include <iomanip>
#include <sstream>
#include <queue>
#include <condition_variable>

namespace file_server {
    struct FileTask {
        std::string file_path;
        evpp::http::ContextPtr ctx;
        evpp::http::HTTPSendResponseCallback cb;
        bool is_dlc;
    };

    FileServer::FileServer() : file_mutex_(), queue_mutex_(), should_exit_(false) {
        std::lock_guard<std::mutex> lock(file_mutex_);

        base_directory_ = utils::configuration::ReadString("Server", "DLCDirectory", "dlc");

        if (base_directory_.empty()) {
            base_directory_ = "dlc";
            utils::configuration::WriteString("Server", "DLCDirectory", base_directory_);
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                "Setting default DLC directory: %s", base_directory_.c_str());
        }
        else {
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

        // Start the worker thread
        worker_thread_ = std::thread(&FileServer::file_worker_thread, this);
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
            "File server worker thread started");
    }

    FileServer::~FileServer() {
        // Signal the worker thread to exit
        {
            std::lock_guard<std::mutex> lock(queue_mutex_);
            should_exit_ = true;
            queue_cv_.notify_one();
        }

        // Wait for the worker thread to finish
        if (worker_thread_.joinable()) {
            worker_thread_.join();
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
            "File server worker thread stopped");
    }

    void FileServer::file_worker_thread() {
        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
            "File server worker thread running");

        while (!should_exit_) {
            FileTask task;
            bool has_task = false;

            // Wait for a task or exit signal
            {
                std::unique_lock<std::mutex> lock(queue_mutex_);
                queue_cv_.wait(lock, [this] {
                    return !task_queue_.empty() || should_exit_;
                    });

                if (should_exit_ && task_queue_.empty()) {
                    break;
                }

                if (!task_queue_.empty()) {
                    task = task_queue_.front();
                    task_queue_.pop();
                    has_task = true;
                }
            }

            // Process the task if we got one
            if (has_task) {
                try {
                    process_file_task(task);
                }
                catch (const std::exception& e) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                        "Error processing file task: %s", e.what());
                }
                catch (...) {
                    logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                        "Unknown error processing file task");
                }
            }
        }

        logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
            "File server worker thread exiting");
    }

    void FileServer::process_file_task(const FileTask& task) {
        read_file(task.file_path, task.ctx, task.cb);
    }

    void FileServer::handle_dlc_download(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

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
            }
            else {
                file_path = "dlc/" + uri;
            }

#ifdef DEBUG
            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                "Attempting to serve static file: %s", file_path.c_str());
#endif

            // Add the task to the queue for the worker thread to process
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                FileTask task;
                task.file_path = file_path;
                task.ctx = ctx;
                task.cb = cb;
                task.is_dlc = true;
                task_queue_.push(task);
            }

            // Notify the worker thread that there's a new task
            queue_cv_.notify_one();
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                "Error in handle_dlc_download: %s", e.what());
            ctx->set_response_http_code(500);
            cb("Internal server error");
        }
    }

    void FileServer::handle_webpanel_file(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
        const evpp::http::HTTPSendResponseCallback& cb) {

        try {
            std::string uri = ctx->uri();
            uri = sanitize_filename(uri);

            std::string file_path = "webpanel" + uri;

            logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                "Original URI: %s, Sanitized URI: %s, Final path: %s",
                ctx->uri().c_str(), uri.c_str(), file_path.c_str());

            // Add the task to the queue for the worker thread to process
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                FileTask task;
                task.file_path = file_path;
                task.ctx = ctx;
                task.cb = cb;
                task.is_dlc = false;
                task_queue_.push(task);
            }

            // Notify the worker thread that there's a new task
            queue_cv_.notify_one();
        }
        catch (const std::exception& e) {
            logger::write(logger::LOG_LEVEL_ERROR, logger::LOG_LABEL_FILESERVER,
                "Error in handle_webpanel_file: %s", e.what());
            ctx->set_response_http_code(500);
            cb("Internal server error");
        }
    }

    void FileServer::read_file(const std::string& file_path,
        const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb) {

        try {
            std::string response_data;
            {
                std::lock_guard<std::mutex> lock(file_mutex_);
                if (std::filesystem::exists(file_path)) {
                    // Get file size for buffer pre-allocation
                    size_t file_size = std::filesystem::file_size(file_path);

                    // Pre-allocate buffer to avoid reallocations
                    response_data.reserve(file_size);

                    // Read file in chunks for better performance
                    std::ifstream file(file_path, std::ios::binary);
                    if (file.is_open()) {
                        const size_t chunk_size = 131072; // 128KB chunks
                        char buffer[chunk_size];

                        while (file) {
                            file.read(buffer, chunk_size);
                            response_data.append(buffer, file.gcount());
                        }
                    }
                }
            }

            if (!response_data.empty()) {
                // Set content type based on file extension
                std::string content_type = "application/octet-stream"; // Default
                std::string file_ext = file_path.substr(file_path.find_last_of(".") + 1);

                if (file_ext == "html" || file_ext == "htm") {
                    content_type = "text/html; charset=utf-8";
                    // Don't cache HTML files - they should be fresh
                    ctx->AddResponseHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                    ctx->AddResponseHeader("Pragma", "no-cache");
                    ctx->AddResponseHeader("Expires", "0");
                }
                else if (file_ext == "js") {
                    content_type = "application/javascript; charset=utf-8";
                    // Don't cache JS files - they should be fresh
                    ctx->AddResponseHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                    ctx->AddResponseHeader("Pragma", "no-cache");
                    ctx->AddResponseHeader("Expires", "0");
                }
                else if (file_ext == "css") {
                    content_type = "text/css; charset=utf-8";
                    // Don't cache CSS files - they should be fresh
                    ctx->AddResponseHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                    ctx->AddResponseHeader("Pragma", "no-cache");
                    ctx->AddResponseHeader("Expires", "0");
                }
                else if (file_ext == "png" || file_ext == "jpg" || file_ext == "jpeg" || file_ext == "gif" || file_ext == "svg") {
                    // Set appropriate image content type
                    if (file_ext == "png") {
                        content_type = "image/png";
                    }
                    else if (file_ext == "jpg" || file_ext == "jpeg") {
                        content_type = "image/jpeg";
                    }
                    else if (file_ext == "gif") {
                        content_type = "image/gif";
                    }
                    else if (file_ext == "svg") {
                        content_type = "image/svg+xml";
                    }

                    // Cache images for 7 days (604800 seconds)
                    ctx->AddResponseHeader("Cache-Control", "public, max-age=604800, immutable");
                    ctx->AddResponseHeader("Expires", FileServer::get_future_date(7)); // 7 days in the future
                }
                else if (file_ext == "json") {
                    content_type = "application/json; charset=utf-8";
                    // Don't cache JSON responses by default
                    ctx->AddResponseHeader("Cache-Control", "no-cache");
                }
                else if (file_ext == "zip" || file_ext == "bin" || file_ext == "dat") {
                    // DLC files
                    if (file_ext == "zip") {
                        content_type = "application/zip";
                    }
                    else if (file_ext == "bin" || file_ext == "dat") {
                        content_type = "application/octet-stream";
                    }

                    // Cache DLC files for 30 days
                    ctx->AddResponseHeader("Cache-Control", "public, max-age=2592000");
                    ctx->AddResponseHeader("Expires", FileServer::get_future_date(30)); // 30 days
                }
                else {
                    // For other file types, don't cache
                    ctx->AddResponseHeader("Cache-Control", "no-cache, no-store, must-revalidate");
                    ctx->AddResponseHeader("Pragma", "no-cache");
                    ctx->AddResponseHeader("Expires", "0");
                }

                // Add performance-enhancing headers
                ctx->AddResponseHeader("Accept-Ranges", "bytes");
                ctx->AddResponseHeader("Connection", "keep-alive");

                // Add ETag header for all files to support conditional requests
                std::string etag = "\"" + std::to_string(response_data.size()) + "-" +
                    std::to_string(std::hash<std::string>{}(file_path)) + "\"";
                ctx->AddResponseHeader("ETag", etag);
                ctx->AddResponseHeader("Content-Type", content_type);

                // Move the response data to avoid copying
                cb(std::move(response_data));

                // Only log non-DLC files or if verbose logging is enabled
                bool is_dlc_file = (file_ext == "zip" || file_ext == "bin" || file_ext == "dat");
                bool verbose_logging = utils::configuration::ReadBoolean("FileServer", "VerboseLogging", false);
                
                if (!is_dlc_file || verbose_logging) {
                    logger::write(logger::LOG_LEVEL_INFO, logger::LOG_LABEL_FILESERVER,
                        "Successfully served static file: %s (Size: %zu bytes)",
                        file_path.c_str(), response_data.size());
                }
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

    std::string FileServer::get_future_date(int days) {
        time_t now = time(0);
        tm* ltm = localtime(&now);
        ltm->tm_mday += days;
        mktime(ltm);
        char buffer[80];
        strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", ltm);
        return std::string(buffer);
    }
}