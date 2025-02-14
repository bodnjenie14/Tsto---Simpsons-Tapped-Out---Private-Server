#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>
#include <evpp/event_loop.h>
#include <future>
#include <queue>

namespace file_server {
    class FileServer {
    public:
        FileServer();
        void handle_dlc_download(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

    private:
        std::string base_directory_;
        std::mutex file_mutex_;  
        std::mutex queue_mutex_; 
        std::queue<std::future<void>> pending_ops_;

        void async_read_file(evpp::EventLoop* loop, const std::string& file_path,
            const evpp::http::ContextPtr& ctx, const evpp::http::HTTPSendResponseCallback& cb);
        bool is_path_safe(const std::string& requested_path) const;
        std::string sanitize_filename(const std::string& filename) const;
        void cleanup_completed_ops();
    };
}