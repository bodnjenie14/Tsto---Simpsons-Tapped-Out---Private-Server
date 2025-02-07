#pragma once
#include "networking/server_startup.hpp"
#include "tsto_server.hpp"
#include <memory>
#include "file_server/file_server.hpp"

namespace server::dispatcher::http {
    class Dispatcher {
    public:
        explicit Dispatcher(std::shared_ptr<tsto::TSTOServer> server);
        void handle(evpp::EventLoop* loop, const evpp::http::ContextPtr& ctx,
            const evpp::http::HTTPSendResponseCallback& cb) noexcept;

    private:
        std::shared_ptr<tsto::TSTOServer> tsto_server_;
        std::unique_ptr<file_server::FileServer> file_server_;
    };
}