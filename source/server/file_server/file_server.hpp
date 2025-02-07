#pragma once
#include <std_include.hpp>
#include <evpp/http/http_server.h>

namespace file_server {
    class FileServer {
    public:
        FileServer();
        void handle_dlc_download(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb);

    private:
        std::string base_directory_;
        bool is_path_safe(const std::string& requested_path) const;
        std::string sanitize_filename(const std::string& filename) const;
    };
}