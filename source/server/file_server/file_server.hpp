#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include <evpp/event_loop.h>
#include <mutex>
#include <thread>
#include <queue>
#include <condition_variable>
#include <atomic>

namespace file_server {
    class FileServer {
    public:
        FileServer();
        ~FileServer();

        void handle_dlc_download(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);
        void handle_webpanel_file(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

    private:
        struct FileTask {
            std::string file_path;
            evpp::http::ContextPtr ctx;
            evpp::http::HTTPSendResponseCallback cb;
            bool is_dlc; // Flag to differentiate between DLC and webpanel files
        };

        std::string base_directory_;
        std::mutex file_mutex_;

        // Thread-safe task queue
        std::queue<FileTask> task_queue_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cv_;

        // Worker thread
        std::thread worker_thread_;
        std::atomic<bool> should_exit_;

        // Worker thread function
        void file_worker_thread();

        // Process a single file task
        void process_file_task(const FileTask& task);

        // Actual file reading implementation
        void read_file(const std::string& file_path,
            const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);

        bool is_path_safe(const std::string& requested_path) const;
        std::string sanitize_filename(const std::string& filename) const;
        static std::string get_future_date(int days);
    };
}
